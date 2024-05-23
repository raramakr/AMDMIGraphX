// Define TRT entrypoints used in common code
// #define DEFINE_TRT_ENTRYPOINTS 1
// #define DEFINE_TRT_LEGACY_PARSER_ENTRYPOINT 0

#include "migraphx/stringutils.hpp"
#include "samples_common/argsParser.hpp"
#include "samples_common/buffers.hpp"
#include "samples_common/common.hpp"

#include "MgxInfer.hpp"
#include <hip/hip_runtime_api.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <iomanip>
#include <sstream>
using namespace mgxinfer1;

class Logger : public mgxinfer1::ILogger
{
    public:
    void log(Severity, mgxinfer1::AsciiChar const*) noexcept override {}
};

const std::string gSampleName = "TensorRT.sample_onnx_mnist";

//! \brief  The SampleOnnxMNIST class implements the ONNX MNIST sample
//!
//! \details It creates the network using an ONNX model
//!
class SampleOnnxMNIST
{
    public:
    SampleOnnxMNIST(const samplesCommon::OnnxSampleParams& params)
        : mParams(params), mRuntime(nullptr), mEngine(nullptr)
    {
    }

    //!
    //! \brief Function builds the network engine
    //!
    bool build();

    //!
    //! \brief Runs the TensorRT inference engine for this sample
    //!
    bool infer();

    private:
    samplesCommon::OnnxSampleParams mParams; //!< The parameters for the sample.

    mgxinfer1::Dims mInputDims;  //!< The dimensions of the input to the network.
    mgxinfer1::Dims mOutputDims; //!< The dimensions of the output to the network.
    int mNumber{0};              //!< The number to classify

    std::shared_ptr<mgxinfer1::IRuntime>
        mRuntime; //!< The TensorRT runtime used to deserialize the engine
    std::shared_ptr<mgxinfer1::ICudaEngine>
        mEngine; //!< The TensorRT engine used to run the network

    //!
    //! \brief Parses an ONNX model for MNIST and creates a TensorRT network
    //!
    bool constructNetwork(std::unique_ptr<mgxinfer1::IBuilder>& builder,
                          std::unique_ptr<mgxinfer1::INetworkDefinition>& network,
                          std::unique_ptr<mgxinfer1::IBuilderConfig>& config,
                          std::unique_ptr<mgxonnxparser::IParser>& parser);

    //!
    //! \brief Reads the input  and stores the result in a managed buffer
    //!
    bool processInput(const samplesCommon::BufferManager& buffers);

    //!
    //! \brief Classifies digits and verify result
    //!
    bool verifyOutput(const samplesCommon::BufferManager& buffers);
};

// //!
// //! \brief Creates the network, configures the builder and creates the network engine
// //!
// //! \details This function creates the Onnx MNIST network by parsing the Onnx model and builds
// //!          the engine that will be used to run MNIST (mEngine)
// //!
// //! \return true if the engine was created successfully and false otherwise
// //!
bool SampleOnnxMNIST::build()
{
    auto logger = std::make_unique<Logger>();

    auto builder = std::unique_ptr<mgxinfer1::IBuilder>(mgxinfer1::createInferBuilder(*logger));
    if(!builder)
    {
        return false;
    }

    auto network = std::unique_ptr<mgxinfer1::INetworkDefinition>(builder->createNetworkV2(0));
    if(!network)
    {
        return false;
    }

    auto config = std::unique_ptr<mgxinfer1::IBuilderConfig>(builder->createBuilderConfig());
    if(!config)
    {
        return false;
    }

    auto parser =
        std::unique_ptr<mgxonnxparser::IParser>(mgxonnxparser::createParser(*network, *logger));
    if(!parser)
    {
        return false;
    }

    auto constructed = constructNetwork(builder, network, config, parser);
    if(!constructed)
    {
        return false;
    }

    // CUDA stream used for profiling by the builder.
    auto profileStream = samplesCommon::makeCudaStream();
    if(!profileStream)
    {
        return false;
    }
    config->setProfileStream(*profileStream);

    std::unique_ptr<IHostMemory> plan{builder->buildSerializedNetwork(*network, *config)};
    if(!plan)
    {
        return false;
    }

    mRuntime = std::shared_ptr<mgxinfer1::IRuntime>(createInferRuntime(*logger));
    if(!mRuntime)
    {
        return false;
    }

    mEngine = std::shared_ptr<mgxinfer1::ICudaEngine>(
        mRuntime->deserializeCudaEngine(plan->data(), plan->size()), samplesCommon::InferDeleter());
    if(!mEngine)
    {
        return false;
    }
    
    std::cout << "BLA" << std::endl;

    ASSERT(network->getNbInputs() == 1);
    mInputDims = network->getInput(0)->getDimensions();
    ASSERT(mInputDims.nbDims == 4);

        std::cout << "ABA" << std::endl;

    ASSERT(network->getNbOutputs() == 1);
    mOutputDims = network->getOutput(0)->getDimensions();
    ASSERT(mOutputDims.nbDims == 2);

    return true;
}

// //!
// //! \brief Uses a ONNX parser to create the Onnx MNIST Network and marks the
// //!        output layers
// //!
// //! \param network Pointer to the network that will be populated with the Onnx MNIST network
// //!
// //! \param builder Pointer to the engine builder
// //!
bool SampleOnnxMNIST::constructNetwork(std::unique_ptr<mgxinfer1::IBuilder>& builder,
                                       std::unique_ptr<mgxinfer1::INetworkDefinition>& network,
                                       std::unique_ptr<mgxinfer1::IBuilderConfig>& config,
                                       std::unique_ptr<mgxonnxparser::IParser>& parser)
{
    auto file   = locateFile(mParams.onnxFileName, mParams.dataDirs);
    auto parsed = parser->parseFromFile(file.c_str(), 0);
    if(!parsed)
    {
        return false;
    }

    if(mParams.fp16)
    {
        config->setFlag(BuilderFlag::kFP16);
    }
    if(mParams.bf16)
    {
        config->setFlag(BuilderFlag::kBF16);
    }
    if(mParams.int8)
    {
        config->setFlag(BuilderFlag::kINT8);
        // samplesCommon::setAllDynamicRanges(network.get(), 127.0F, 127.0F);
    }

    // samplesCommon::enableDLA(builder.get(), config.get(), mParams.dlaCore);

    return true;
}

//!
//! \brief Runs the TensorRT inference engine for this sample
//!
//! \details This function is the main execution function of the sample. It allocates the buffer,
//!          sets inputs and executes the engine.
//!
bool SampleOnnxMNIST::infer()
{
    // Create RAII buffer manager object
    samplesCommon::BufferManager buffers(mEngine);
    auto context = std::unique_ptr<mgxinfer1::IExecutionContext>(mEngine->createExecutionContext());
    if(!context)
    {
        return false;
    }

    for (int32_t i = 0, e = mEngine->getNbIOTensors(); i < e; i++)
    {
        auto const name = mEngine->getIOTensorName(i);
        context->setTensorAddress(name, buffers.getDeviceBuffer(name));
    }

    // Read the input data into the managed buffers
    std::cout << mParams.inputTensorNames.size() << std::endl;
    std::cout << migraphx::to_string_range(mParams.inputTensorNames) << std::endl;
    // ASSERT(mParams.inputTensorNames.size() == 1);
    if (!processInput(buffers))
    {
        return false;
    }

    // Memcpy from host input buffers to device input buffers
    buffers.copyInputToDevice();

    bool status = context->executeV2(buffers.getDeviceBindings().data());
    if (!status)
    {
        return false;
    }

    // Memcpy from device output buffers to host output buffers
    buffers.copyOutputToHost();


    // Verify results
    if (!verifyOutput(buffers))
    {
        return false;
    }

    return true;
}

//!
//! \brief Reads the input and stores the result in a managed buffer
//!
bool SampleOnnxMNIST::processInput(const samplesCommon::BufferManager& buffers)
{
    const int inputH = mInputDims.d[2];
    const int inputW = mInputDims.d[3];

    // Read a random digit file
    srand(unsigned(time(nullptr)));
    std::vector<uint8_t> fileData(inputH * inputW);
    mNumber = rand() % 10;
    readPGMFile(locateFile(std::to_string(mNumber) + ".pgm", mParams.dataDirs), fileData.data(),
    inputH, inputW);

    // Print an ascii representation
    std::cout << "Input:" << std::endl;
    for (int i = 0; i < inputH * inputW; i++)
    {
        std::cout << (" .:-=+*#%@"[fileData[i] / 26]) << (((i + 1) % inputW) ? "" : "\n");
    }
    std::cout << std::endl;

    float* hostDataBuffer =
    static_cast<float*>(buffers.getHostBuffer(mParams.inputTensorNames[0])); for (int i = 0; i <
    inputH * inputW; i++)
    {
        hostDataBuffer[i] = 1.0 - float(fileData[i] / 255.0);
    }

    return true;
}

//!
//! \brief Classifies digits and verify result
//!
//! \return whether the classification output matches expectations
//!
bool SampleOnnxMNIST::verifyOutput(const samplesCommon::BufferManager& buffers)
{
    const int outputSize = mOutputDims.d[1];
    float* output = static_cast<float*>(buffers.getHostBuffer(mParams.outputTensorNames[0]));
    float val{0.0F};
    int idx{0};

    // Calculate Softmax
    float sum{0.0F};
    for (int i = 0; i < outputSize; i++)
    {
        output[i] = exp(output[i]);
        sum += output[i];
    }

    std::cout << "Output:" << std::endl;
    for (int i = 0; i < outputSize; i++)
    {
        output[i] /= sum;
        val = std::max(val, output[i]);
        if (val == output[i])
        {
            idx = i;
        }

        std::cout << " Prob " << i << "  " << std::fixed << std::setw(5) <<
        std::setprecision(4) << output[i]
                         << " "
                         << "Class " << i << ": " << std::string(int(std::floor(output[i] * 10 +
                         0.5F)), '*')
                         << std::endl;
    }
    std::cout << std::endl;

    return idx == mNumber && val > 0.9F;
}

// //!
// //! \brief Initializes members of the params struct using the command line args
// //!
samplesCommon::OnnxSampleParams initializeSampleParams(const samplesCommon::Args& args)
{
    samplesCommon::OnnxSampleParams params;
    if(args.dataDirs.empty()) // Use default directories if user hasn't provided directory paths
    {
        params.dataDirs.push_back("data/mnist/");
        params.dataDirs.push_back("data/samples/mnist/");
    }
    else // Use the data directory provided by the user
    {
        params.dataDirs = args.dataDirs;
    }
    params.onnxFileName = "mnist.onnx";
    params.inputTensorNames.push_back("Input3");
    params.outputTensorNames.push_back("main:#output_0"); // guessing output name?
    params.dlaCore = args.useDLACore;
    params.int8    = args.runInInt8;
    params.fp16    = args.runInFp16;
    params.bf16    = args.runInBf16;

    return params;
}

//!
//! \brief Prints the help information for running this sample
//!
void printHelpInfo()
{
    std::cout << "Usage: ./sample_onnx_mnist [-h or --help] [-d or --datadir=<path to data "
                 "directory>] [--useDLACore=<int>]"
              << std::endl;
    std::cout << "--help          Display help information" << std::endl;
    std::cout << "--datadir       Specify path to a data directory, overriding the default. This "
                 "option can be used "
                 "multiple times to add multiple directories. If no data directories are given, "
                 "the default is to use "
                 "(data/samples/mnist/, data/mnist/)"
              << std::endl;
    std::cout << "--useDLACore=N  Specify a DLA engine for layers that support DLA. Value can "
                 "range from 0 to n-1, "
                 "where n is the number of DLA engines on the platform."
              << std::endl;
    std::cout << "--int8          Run in Int8 mode." << std::endl;
    std::cout << "--fp16          Run in FP16 mode." << std::endl;
    std::cout << "--bf16          Run in BF16 mode." << std::endl;
}

int main(int argc, char** argv)
{
    samplesCommon::Args args;
    bool argsOK = samplesCommon::parseArgs(args, argc, argv);
    if(!argsOK)
    {
        std::cerr << "Invalid arguments" << std::endl;
        printHelpInfo();
        return EXIT_FAILURE;
    }
    if(args.help)
    {
        printHelpInfo();
        return EXIT_SUCCESS;
    }

    // auto sampleTest = sample::gLogger.defineTest(gSampleName, argc, argv);

    // sample::gLogger.reportTestStart(sampleTest);

    SampleOnnxMNIST sample(initializeSampleParams(args));

    // sample::gLogInfo << "Building and running a GPU inference engine for Onnx MNIST" <<
    // std::endl;

    if(!sample.build())
    {
        // return sample::gLogger.reportFail(sampleTest);
        std::cerr << "Build failed" << std::endl;
        return 1;
    }
    if(!sample.infer())
    {
        // return sample::gLogger.reportFail(sampleTest);
        std::cerr << "Infer failed" << std::endl;
        return 1;
    }

    // return sample::gLogger.reportPass(sampleTest);
    return 0;
}
