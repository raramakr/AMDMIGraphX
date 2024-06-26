#pragma once

#include <iterator>
#include <migraphx/shape.hpp>

#include <algorithm>
#include <cstdint>

namespace mgxinfer1 {

namespace impl {
//! Declaration of EnumMaxImpl struct to store maximum number of elements in an enumeration type.
template <typename T>
struct EnumMaxImpl;
} // namespace impl

//! Maximum number of elements in an enumeration type.
template <typename T>
constexpr int32_t EnumMax() noexcept
{
    return impl::EnumMaxImpl<T>::kVALUE;
}

//! char_t is the type used by TensorRT to represent all valid characters.
using char_t = char;

//! AsciiChar is the type used by TensorRT to represent valid ASCII characters.
//! This type is widely used in automotive safety context.
using AsciiChar = char_t;

//!
//! \enum DataType
//! \brief The type of weights and tensors.
//!
enum class DataType : int32_t
{
    //! 32-bit floating point format.
    kFLOAT = 0,

    //! IEEE 16-bit floating-point format -- has a 5 bit exponent and 11 bit significand.
    kHALF = 1,

    //! Signed 8-bit integer representing a quantized floating-point value.
    kINT8 = 2,

    //! Signed 32-bit integer format.
    kINT32 = 3,

    //! 8-bit boolean. 0 = false, 1 = true, other values undefined.
    kBOOL = 4,

    //! Unsigned 8-bit integer format.
    //! Cannot be used to represent quantized floating-point values.
    //! Use the IdentityLayer to convert kUINT8 network-level inputs to {kFLOAT, kHALF} prior
    //! to use with other TensorRT layers, or to convert intermediate output
    //! before kUINT8 network-level outputs from {kFLOAT, kHALF} to kUINT8.
    //! kUINT8 conversions are only supported for {kFLOAT, kHALF}.
    //! kUINT8 to {kFLOAT, kHALF} conversion will convert the integer values
    //! to equivalent floating point values.
    //! {kFLOAT, kHALF} to kUINT8 conversion will convert the floating point values
    //! to integer values by truncating towards zero. This conversion has undefined behavior for
    //! floating point values outside the range [0.0F, 256.0F) after truncation.
    //! kUINT8 conversions are not supported for {kINT8, kINT32, kBOOL}.
    kUINT8 = 5,

    //! Signed 8-bit floating point with
    //! 1 sign bit, 4 exponent bits, 3 mantissa bits, and exponent-bias 7.
    kFP8 = 6,

    //! Brain float -- has an 8 bit exponent and 8 bit significand.
    kBF16 = 7,

    //! Signed 64-bit integer type.
    kINT64 = 8,

    //! Signed 4-bit integer type.
    kINT4 = 9,
};

inline size_t sizeofDataType(DataType type)
{
    switch(type)
    {
    case mgxinfer1::DataType::kFLOAT: return 4;
    case mgxinfer1::DataType::kHALF: return 2;
    case mgxinfer1::DataType::kINT8: return 1;
    case mgxinfer1::DataType::kINT32: return 4;
    case mgxinfer1::DataType::kINT64: return 8;
    case mgxinfer1::DataType::kBOOL: return 1;
    case mgxinfer1::DataType::kUINT8: return 1;
    case mgxinfer1::DataType::kFP8: return 1;
    case mgxinfer1::DataType::kBF16: return 2;
    // Not implemented in TRT
    case mgxinfer1::DataType::kINT4: break;
    }
    return -1;
}

//!
//! \class Dims
//! \brief Structure to define the dimensions of a tensor.
//!
//! TensorRT can also return an "invalid dims" structure. This structure is
//! represented by nbDims == -1 and d[i] == 0 for all i.
//!
//! TensorRT can also return an "unknown rank" dims structure. This structure is
//! represented by nbDims == -1 and d[i] == -1 for all i.
//!
class Dims64
{
    public:
    //! The maximum rank (number of dimensions) supported for a tensor.
    static constexpr int32_t MAX_DIMS{8};

    //! The rank (number of dimensions).
    int32_t nbDims;

    //! The extent of each dimension.
    int64_t d[MAX_DIMS];
};

//!
//! Alias for Dims64.
//!
using Dims = Dims64;

class Dims2 : public Dims
{
public:
    //!
    //! \brief Construct an empty Dims2 object.
    //!
    Dims2()
        : Dims2(0, 0)
    {
    }

    //!
    //! \brief Construct a Dims2 from 2 elements.
    //!
    //! \param d0 The first element.
    //! \param d1 The second element.
    //!
    Dims2(int64_t d0, int64_t d1)
    {
        nbDims = 2;
        d[0] = d0;
        d[1] = d1;
        for (int64_t i{nbDims}; i < Dims::MAX_DIMS; ++i)
        {
            d[i] = 0;
        }
    }
};

//!
//! \class DimsHW
//!
//! \brief Descriptor for two-dimensional spatial data.
//!
class DimsHW : public Dims2
{
public:
    //!
    //! \brief Construct an empty DimsHW object.
    //!
    DimsHW()
        : Dims2()
    {
    }

    //!
    //! \brief Construct a DimsHW given height and width.
    //!
    //! \param height the height of the data
    //! \param width the width of the data
    //!
    DimsHW(int64_t height, int64_t width)
        : Dims2(height, width)
    {
    }

    //!
    //! \brief Get the height.
    //!
    //! \return The height.
    //!
    int64_t& h()
    {
        return d[0];
    }

    //!
    //! \brief Get the height.
    //!
    //! \return The height.
    //!
    int64_t h() const
    {
        return d[0];
    }

    //!
    //! \brief Get the width.
    //!
    //! \return The width.
    //!
    int64_t& w()
    {
        return d[1];
    }

    //!
    //! \brief Get the width.
    //!
    //! \return The width.
    //!
    int64_t w() const
    {
        return d[1];
    }
};

//!
//! \class Dims3
//!
//! \brief Descriptor for three-dimensional data.
//!
class Dims3 : public Dims2
{
public:
    //!
    //! \brief Construct an empty Dims3 object.
    //!
    Dims3()
        : Dims3(0, 0, 0)
    {
    }

    //!
    //! \brief Construct a Dims3 from 3 elements.
    //!
    //! \param d0 The first element.
    //! \param d1 The second element.
    //! \param d2 The third element.
    //!
    Dims3(int64_t d0, int64_t d1, int64_t d2)
        : Dims2(d0, d1)
    {
        nbDims = 3;
        d[2] = d2;
    }
};

//!
//! \class Dims4
//!
//! \brief Descriptor for four-dimensional data.
//!
class Dims4 : public Dims3
{
public:
    //!
    //! \brief Construct an empty Dims4 object.
    //!
    Dims4()
        : Dims4(0, 0, 0, 0)
    {
    }

    //!
    //! \brief Construct a Dims4 from 4 elements.
    //!
    //! \param d0 The first element.
    //! \param d1 The second element.
    //! \param d2 The third element.
    //! \param d3 The fourth element.
    //!
    Dims4(int64_t d0, int64_t d1, int64_t d2, int64_t d3)
        : Dims3(d0, d1, d2)
    {
        nbDims = 4;
        d[3] = d3;
    }
};

//!
//! \struct Permutation
//!
//! \brief Represents a permutation of dimensions.
//!
struct Permutation
{
    //!
    //! The elements of the permutation.
    //! The permutation is applied as outputDimensionIndex = permutation.order[inputDimensionIndex], so to
    //! permute from CHW order to HWC order, the required permutation is [1, 2, 0], and to permute
    //! from HWC to CHW, the required permutation is [2, 0, 1].
    //!
    int32_t order[Dims::MAX_DIMS];
};

inline int64_t volume(const Dims& dims)
{
    int64_t vol = 1;
    for(auto i = 0; i < dims.nbDims; ++i)
        vol *= dims.d[i];
    return vol;
}

//!
//! \enum TensorFormat
//!
//! \brief Format of the input/output tensors.
//!
//! This enum is used by both plugins and network I/O tensors.
//!
//! \see IPluginV2::supportsFormat(), safe::ICudaEngine::getBindingFormat()
//!
//! Many of the formats are **vector-major** or **vector-minor**. These formats specify
//! a <em>vector dimension</em> and <em>scalars per vector</em>.
//! For example, suppose that the tensor has has dimensions [M,N,C,H,W],
//! the vector dimension is C and there are V scalars per vector.
//!
//! * A **vector-major** format splits the vectorized dimension into two axes in the
//!   memory layout. The vectorized dimension is replaced by an axis of length ceil(C/V)
//!   and a new dimension of length V is appended. For the example tensor, the memory layout
//!   is equivalent to an array with dimensions [M][N][ceil(C/V)][H][W][V].
//!   Tensor coordinate (m,n,c,h,w) maps to array location [m][n][c/V][h][w][c\%V].
//!
//! * A **vector-minor** format moves the vectorized dimension to become the last axis
//!   in the memory layout. For the example tensor, the memory layout is equivalent to an
//!   array with dimensions [M][N][H][W][ceil(C/V)*V]. Tensor coordinate (m,n,c,h,w) maps
//!   array location subscript [m][n][h][w][c].
//!
//! In interfaces that refer to "components per element", that's the value of V above.
//!
//! For more information about data formats, see the topic "Data Format Description" located in the
//! TensorRT Developer Guide.
//! https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#data-format-desc
//!
enum class TensorFormat : int32_t
{
    //! Memory layout is similar to an array in C or C++.
    //! The stride of each dimension is the product of the dimensions after it.
    //! The last dimension has unit stride.
    //!
    //! For DLA usage, the tensor sizes are limited to C,H,W in the range [1,8192].
    kLINEAR = 0,

    //! Vector-major format with two scalars per vector.
    //! Vector dimension is third to last.
    //!
    //! This format requires FP16 or BF16 and at least three dimensions.
    kCHW2 = 1,

    //! Vector-minor format with eight scalars per vector.
    //! Vector dimension is third to last.
    //! This format requires FP16 or BF16 and at least three dimensions.
    kHWC8 = 2,

    //! Vector-major format with four scalars per vector.
    //! Vector dimension is third to last.
    //!
    //! This format requires INT8 or FP16 and at least three dimensions.
    //! For INT8, the length of the vector dimension must be a build-time constant.
    //!
    //! Deprecated usage:
    //!
    //! If running on the DLA, this format can be used for acceleration
    //! with the caveat that C must be less than or equal to 4.
    //! If used as DLA input and the build option kGPU_FALLBACK is not specified,
    //! it needs to meet line stride requirement of DLA format. Column stride in
    //! bytes must be a multiple of 64 on Orin.
    kCHW4 = 3,

    //! Vector-major format with 16 scalars per vector.
    //! Vector dimension is third to last.
    //!
    //! This format requires INT8 or FP16 and at least three dimensions.
    //!
    //! For DLA usage, this format maps to the native feature format for FP16,
    //! and the tensor sizes are limited to C,H,W in the range [1,8192].
    kCHW16 = 4,

    //! Vector-major format with 32 scalars per vector.
    //! Vector dimension is third to last.
    //!
    //! This format requires at least three dimensions.
    //!
    //! For DLA usage, this format maps to the native feature format for INT8,
    //! and the tensor sizes are limited to C,H,W in the range [1,8192].
    kCHW32 = 5,

    //! Vector-minor format with eight scalars per vector.
    //! Vector dimension is fourth to last.
    //!
    //! This format requires FP16 or BF16 and at least four dimensions.
    kDHWC8 = 6,

    //! Vector-major format with 32 scalars per vector.
    //! Vector dimension is fourth to last.
    //!
    //! This format requires FP16 or INT8 and at least four dimensions.
    kCDHW32 = 7,

    //! Vector-minor format where channel dimension is third to last and unpadded.
    //!
    //! This format requires either FP32 or UINT8 and at least three dimensions.
    kHWC = 8,

    //! DLA planar format. For a tensor with dimension {N, C, H, W}, the W axis
    //! always has unit stride. The stride for stepping along the H axis is
    //! rounded up to 64 bytes.
    //!
    //! The memory layout is equivalent to a C array with dimensions
    //! [N][C][H][roundUp(W, 64/elementSize)] where elementSize is
    //! 2 for FP16 and 1 for Int8, with the tensor coordinates (n, c, h, w)
    //! mapping to array subscript [n][c][h][w].
    kDLA_LINEAR = 9,

    //! DLA image format. For a tensor with dimension {N, C, H, W} the C axis
    //! always has unit stride. The stride for stepping along the H axis is rounded up
    //! to 64 bytes on Orin. C can only be 1, 3 or 4.
    //! If C == 1, it will map to grayscale format.
    //! If C == 3 or C == 4, it will map to color image format. And if C == 3,
    //! the stride for stepping along the W axis needs to be padded to 4 in elements.
    //!
    //! When C is {1, 3, 4}, then C' is {1, 4, 4} respectively,
    //! the memory layout is equivalent to a C array with dimensions
    //! [N][H][roundUp(W, 64/C'/elementSize)][C'] on Orin
    //! where elementSize is 2 for FP16
    //! and 1 for Int8. The tensor coordinates (n, c, h, w) mapping to array
    //! subscript [n][h][w][c].
    kDLA_HWC4 = 10,

    //! Vector-minor format with 16 scalars per vector.
    //! Vector dimension is third to last.
    //!
    //! This requires FP16 and at least three dimensions.
    kHWC16 = 11,

    //! Vector-minor format with one scalar per vector.
    //! Vector dimension is fourth to last.
    //!
    //! This format requires FP32 and at least four dimensions.
    kDHWC = 12
};

//!
//! \class ILogger
//!
//! \brief Application-implemented logging interface for the builder, refitter and runtime.
//!
//! The logger used to create an instance of IBuilder, IRuntime or IRefitter is used for all objects
//! created through that interface. The logger must be valid until all objects created are released.
//!
//! The Logger object implementation must be thread safe. All locking and synchronization is pushed
//! to the interface implementation and TensorRT does not hold any synchronization primitives when
//! calling the interface functions.
//!
class ILogger
{
    public:
    //!
    //! \enum Severity
    //!
    //! \brief The severity corresponding to a log message.
    //!
    enum class Severity : int32_t
    {
        //! An internal error has occurred. Execution is unrecoverable.
        kINTERNAL_ERROR = 0,
        //! An application error has occurred.
        kERROR = 1,
        //! An application error has been discovered, but TensorRT has recovered or fallen back to a
        //! default.
        kWARNING = 2,
        //!  Informational messages with instructional information.
        kINFO = 3,
        //!  Verbose messages with debugging information.
        kVERBOSE = 4,
    };

    //!
    //! \brief A callback implemented by the application to handle logging messages;
    //!
    //! \param severity The severity of the message.
    //! \param msg A null-terminated log message.
    //!
    //! \warning Loggers used in the safety certified runtime must set a maximum message length and
    //! truncate
    //!          messages exceeding this length. It is up to the implementer of the derived class to
    //!          define a suitable limit that will prevent buffer overruns, resource exhaustion, and
    //!          other security vulnerabilities in their implementation. The TensorRT safety
    //!          certified runtime will never emit messages longer than 1024 bytes.
    //!
    //! \usage
    //! - Allowed context for the API call
    //!   - Thread-safe: Yes, this method is required to be thread-safe and may be called from
    //!   multiple threads
    //!                  when multiple execution contexts are used during runtime, or if the same
    //!                  logger is used for multiple runtimes, builders, or refitters.
    //!
    virtual void log(Severity severity, AsciiChar const* msg) noexcept = 0;

    ILogger()          = default;
    virtual ~ILogger() = default;

    protected:
    // @cond SuppressDoxyWarnings
    ILogger(ILogger const&)              = default;
    ILogger(ILogger&&)                   = default;
    ILogger& operator=(ILogger const&) & = default;
    ILogger& operator=(ILogger&&) &      = default;
    // @endcond
};

//!
//! \enum TensorIOMode
//!
//! \brief Definition of tensor IO Mode.
//!
enum class TensorIOMode : int32_t
{
    //! Tensor is not an input or output.
    kNONE = 0,

    //! Tensor is input to the engine.
    kINPUT = 1,

    //! Tensor is output by the engine.
    kOUTPUT = 2
};

//!
//! \enum TensorLocation
//!
//! \brief The location for tensor data storage, device or host.
//!
enum class TensorLocation : int32_t
{
    kDEVICE = 0, //!< Data stored on device.
    kHOST   = 1, //!< Data stored on host.
};

//!
//! \enum WeightsRole
//!
//! \brief How a layer uses particular Weights.
//!
//! The power weights of an IScaleLayer are omitted.  Refitting those is not supported.
//!
enum class WeightsRole : int32_t
{
    kKERNEL   = 0, //!< kernel for IConvolutionLayer or IDeconvolutionLayer
    kBIAS     = 1, //!< bias for IConvolutionLayer or IDeconvolutionLayer
    kSHIFT    = 2, //!< shift part of IScaleLayer
    kSCALE    = 3, //!< scale part of IScaleLayer
    kCONSTANT = 4, //!< weights for IConstantLayer
    kANY      = 5, //!< Any other weights role
};

//! Maximum number of elements in WeightsRole enum. \see WeightsRole
template <>
constexpr inline int32_t EnumMax<WeightsRole>() noexcept
{
    return 6;
}

inline DataType toDataType(const migraphx::shape::type_t& type)
{
    switch(type)
    {
    case migraphx::shape::type_t::float_type: return DataType::kFLOAT;
    case migraphx::shape::type_t::half_type: return DataType::kHALF;
    case migraphx::shape::type_t::int8_type: return DataType::kINT8;
    case migraphx::shape::type_t::int32_type: return DataType::kINT32;
    case migraphx::shape::type_t::bool_type: return DataType::kBOOL;
    case migraphx::shape::type_t::uint8_type: return DataType::kUINT8;
    case migraphx::shape::type_t::fp8e4m3fnuz_type: return DataType::kFP8;
    case migraphx::shape::type_t::int64_type: return DataType::kINT64;
    default: MIGRAPHX_THROW("Type not supported");
    }
}

inline migraphx::shape::type_t fromDataType(const DataType& type)
{
    switch(type)
    {
    case DataType::kFLOAT: return migraphx::shape::type_t::float_type;
    case DataType::kHALF: return migraphx::shape::type_t::half_type;
    case DataType::kINT8: return migraphx::shape::type_t::int8_type;
    case DataType::kINT32: return migraphx::shape::type_t::int32_type;
    case DataType::kBOOL: return migraphx::shape::type_t::bool_type;
    case DataType::kUINT8: return migraphx::shape::type_t::uint8_type;
    case DataType::kFP8: return migraphx::shape::type_t::fp8e4m3fnuz_type;
    case DataType::kINT64: return migraphx::shape::type_t::int64_type;
    default: MIGRAPHX_THROW("Type not supported");
    }
}

inline Dims toDimensions(const migraphx::shape& shape)
{
    Dims dims;
    auto lens   = shape.lens();
    dims.nbDims = static_cast<int32_t>(lens.size());
    std::transform(
        lens.begin(), lens.end(), dims.d, [](auto l) { return static_cast<int64_t>(l); });
    return dims;
}

inline std::vector<int64_t> dimsToVec(const Dims& dims)
{
    std::vector<int64_t> ret;
    std::copy(dims.d, dims.d + dims.nbDims, std::back_inserter(ret));
    return ret;
}

inline std::vector<int64_t> permToVec(const Permutation& perm, int n)
{
    std::vector<int64_t> ret;
    std::copy(perm.order, perm.order + n, std::back_inserter(ret));
    return ret;
}

inline std::vector<int64_t> axesToVector(int32_t axes)
{
    std::vector<int64_t> ret;
    for(int i = 0; i < 32; ++i)
    {
        if(axes & (1 << i))
        {
            ret.push_back(i);
        }
    }
    return ret;
}

} // namespace mgxinfer1
