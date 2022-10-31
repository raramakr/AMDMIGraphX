/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef MIGRAPHX_GUARD_RTGLIB_CONVOLUTION_HPP
#define MIGRAPHX_GUARD_RTGLIB_CONVOLUTION_HPP

#include <migraphx/shape.hpp>
#include <migraphx/generate.hpp>
#include <migraphx/operation.hpp>
#include <migraphx/register_op.hpp>
#include <migraphx/gpu/miopen.hpp>
#include <migraphx/op/identity.hpp>
#include <migraphx/op/convolution.hpp>
#include <migraphx/op/quant_convolution.hpp>
#include <migraphx/op/deconvolution.hpp>
#include <miopen/miopen.h>
#include <unordered_map>
#include <migraphx/reflect.hpp>
#include <migraphx/gpu/context.hpp>
namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace gpu {

inline shape reshape_if_1d(const shape& input)
{
    shape new_shape{input};
    auto dims = new_shape.lens();

    if(dims.size() == 3)
    {
        std::vector<size_t> new_dims = dims;
        new_dims.insert(new_dims.begin() + 2, 1);
        new_shape = shape{input.type(), new_dims};
    }
    return new_shape;
}

template <class Op>
struct miopen_convolution
{
    Op op;
    bool int8_x4_format               = false;
    shared<convolution_descriptor> cd = nullptr;
    miopenConvFwdAlgorithm_t algo{};
#ifdef MIGRAPHX_HAS_FIND_2_API
    value::binary solution_object{};
    shared<miopen_solution> solution_ptr = nullptr;
#endif
    uint64_t solution_id = 0;

    template <class Self, class F>
    static auto reflect(Self& self, F f)
    {
        return pack(f(self.op, "op"),
#ifdef MIGRAPHX_HAS_FIND_2_API
                    f(self.solution_object, "solution_object"),
#endif
                    f(self.algo, "algo"),
                    f(self.int8_x4_format, "int8_x4_format"),
                    f(self.solution_id, "solution_id"));
    }

    std::string name() const { return "gpu::" + op.name(); }

    inline shape compute_shape(const std::vector<shape>& inputs) const
    {
        check_shapes{inputs, op}.has(4).standard();
        std::vector<shape> conv_inputs(inputs.begin(), inputs.begin() + 2);
        check_shapes{conv_inputs, op}.max_ndims(5);
        return migraphx::compute_shape<Op>(op, conv_inputs);
    }

    argument
    compute(context& ctx, const shape& output_shape, const std::vector<argument>& args) const
    {
        auto x_desc = make_tensor(reshape_if_1d(args[0].get_shape()), int8_x4_format);
        auto w_desc = make_tensor(reshape_if_1d(args[1].get_shape()), int8_x4_format);
        auto y_desc = make_tensor(reshape_if_1d(output_shape));
        auto* miopen_stream_handle = ctx.get_stream().get_miopen();
        auto workspace_size        = args[2].get_shape().bytes();

#ifdef MIGRAPHX_HAS_FIND_2_API
        {
            const miopenTensorArgument_t tensor_args[3] = {
                {miopenTensorConvolutionX, nullptr, args[0].implicit()},
                {miopenTensorConvolutionW, nullptr, args[1].implicit()},
                {miopenTensorConvolutionY, nullptr, args[3].implicit()},
            };

            if(solution_ptr.get() == nullptr)
                MIGRAPHX_THROW("MIOpen " + op.name() + " : Load MIOpen Solution before running it");

            auto status = miopenRunSolution(miopen_stream_handle,
                                            solution_ptr.get(),
                                            3,
                                            tensor_args,
                                            args[2].implicit(),
                                            workspace_size);
            if(status != miopenStatusSuccess)
                MIGRAPHX_THROW("MIOpen " + op.name() +
                               " : running convolution using find_2.0 failed");

            return args[3];
        }
#else
        // else use immediate mode
        if(solution_id == 0)
            MIGRAPHX_THROW("MIOpen " + op.name() + " : invalid solution ID");

        auto status = miopenConvolutionForwardImmediate(miopen_stream_handle,
                                                        w_desc.get(),
                                                        args[1].implicit(),
                                                        x_desc.get(),
                                                        args[0].implicit(),
                                                        cd.get(),
                                                        y_desc.get(),
                                                        args[3].implicit(),
                                                        args[2].implicit(),
                                                        workspace_size,
                                                        solution_id);

        if(status != miopenStatusSuccess)
            MIGRAPHX_THROW("MIOpen " + op.name() + ": running convolution failed");
        return args[3];
#endif
    }

    inline void set_conv_descriptor()
    {
        if(cd == nullptr)
        {
            cd = (op.name() == "deconvolution") ? make_deconv(op) : make_conv(op);
        }
    }

    value compile(migraphx::context& ctx, const shape& output, const std::vector<shape>& input)
    {
        set_conv_descriptor();
        auto ws = find(any_cast<migraphx::gpu::context>(ctx), output, input);
        return {{"workspace", ws.bytes()}};
    }

    shape find(context& ctx, const shape& output_shape, const std::vector<shape>& inputs)
    {
        shape workspace_shape{};
        auto x_desc                = make_tensor(reshape_if_1d(inputs[0]), int8_x4_format);
        auto w_desc                = make_tensor(reshape_if_1d(inputs[1]), int8_x4_format);
        auto y_desc                = make_tensor(reshape_if_1d(output_shape));
        std::size_t workspace_size = 0;
#ifdef MIGRAPHX_HAS_FIND_2_API
        {
            auto conv_problem = make_obj<miopen_problem>(
                &miopenCreateConvProblem, cd.get(), miopenProblemDirectionForward);

            set_tensor_descriptor(miopenTensorConvolutionX, x_desc, conv_problem);
            set_tensor_descriptor(miopenTensorConvolutionW, w_desc, conv_problem);
            set_tensor_descriptor(miopenTensorConvolutionY, y_desc, conv_problem);

            auto* miopen_stream_handle = ctx.get_stream().get_miopen();

            solution_ptr = find_solution(miopen_stream_handle, conv_problem.get());
            auto status  = miopenGetSolutionWorkspaceSize(solution_ptr.get(), &workspace_size);
            if(status != miopenStatusSuccess)
                MIGRAPHX_THROW("MIOpen" + op.name() + " : failed to get solution's workspace size");

            std::size_t solution_size;
            status = miopenGetSolutionSize(solution_ptr.get(), &solution_size);
            if(status != miopenStatusSuccess)
                MIGRAPHX_THROW("MIOpen" + op.name() + ": Failed to fetch solution size");

            auto solution_binary = std::vector<char>{};
            solution_binary.resize(solution_size);

            status = miopenSaveSolution(solution_ptr.get(), solution_binary.data());
            if(status != miopenStatusSuccess)
                MIGRAPHX_THROW("MIOpen" + op.name() + ": Saving solution failed");
            solution_object = value::binary{solution_binary.data(), solution_size};
            return shape{shape::int8_type, {workspace_size}};
        }
#else
        auto status = miopenConvolutionForwardGetWorkSpaceSize(ctx.get_stream().get_miopen(),
                                                               w_desc.get(),
                                                               x_desc.get(),
                                                               cd.get(),
                                                               y_desc.get(),
                                                               &workspace_size);
        if(status != miopenStatusSuccess)
            MIGRAPHX_THROW("MIOpen" + op.name() + " : Failed to get forward workspace size");

        workspace_shape = shape{shape::int8_type, {workspace_size}};

        auto x_shape = inputs[0];
        auto w_shape = inputs[1];
        if(int8_x4_format)
        {
            x_shape = pack_int8_shape(x_shape);
            w_shape = pack_int8_shape(w_shape);
        }
        auto x         = to_gpu(generate_argument(x_shape));
        auto w         = to_gpu(generate_argument(w_shape));
        auto y         = allocate_gpu(output_shape);
        auto workspace = allocate_gpu(workspace_shape);

        int algo_count = 1;
        miopenConvAlgoPerf_t perf;
        status = miopenFindConvolutionForwardAlgorithm(ctx.get_stream().get_miopen(),
                                                       x_desc.get(),
                                                       x.implicit(),
                                                       w_desc.get(),
                                                       w.implicit(),
                                                       cd.get(),
                                                       y_desc.get(),
                                                       y.implicit(),
                                                       1,
                                                       &algo_count,
                                                       &perf,
                                                       workspace.implicit(),
                                                       workspace_size,
                                                       false);
        if(status != miopenStatusSuccess)
            MIGRAPHX_THROW("MIOpen " + op.name() + " : find convolution failed");
        algo = perf.fwd_algo;

        size_t solution_count;

        status = miopenConvolutionForwardGetSolutionCount(ctx.get_stream().get_miopen(),
                                                          w_desc.get(),
                                                          x_desc.get(),
                                                          cd.get(),
                                                          y_desc.get(),
                                                          &solution_count);
        if(status != miopenStatusSuccess)
            MIGRAPHX_THROW("MIOpen " + op.name() + ": get solution count failed");

        std::vector<miopenConvSolution_t> solutions(solution_count);

        status = miopenConvolutionForwardGetSolution(ctx.get_stream().get_miopen(),
                                                     w_desc.get(),
                                                     x_desc.get(),
                                                     cd.get(),
                                                     y_desc.get(),
                                                     solution_count,
                                                     &solution_count,
                                                     solutions.data());
        if(status != miopenStatusSuccess)
            MIGRAPHX_THROW("MIOpen " + op.name() + ": get solution failed");

        solution_id = solutions.front().solution_id;

        return shape{shape::int8_type, {perf.memory}};
#endif
    }

    void finalize(context& ctx, const shape& output_shape, const std::vector<shape>& inputs)
    {
#ifdef MIGRAPHX_HAS_FIND_2_API
        {
            (void)(ctx); // avoid warnings
            (void)(output_shape);
            (void)(inputs);
            // load solution
            if(solution_ptr == nullptr)
            {
                miopenSolution_t ptr;
                auto status =
                    miopenLoadSolution(&ptr,
                                       reinterpret_cast<const char*>(solution_object.data()),
                                       solution_object.size());
                solution_ptr = miopen_solution{ptr};
                if(status != miopenStatusSuccess)
                    MIGRAPHX_THROW("MIOpen " + op.name() + ": loading convolution solution failed");
                if(status == miopenStatusVersionMismatch)
                {
                    std::clog << "MIOpen convolution was compiled with different MIOpen version. "
                                 "But this machine has MIOpen version: "
                              << get_miopen_version() << ", Performance may suffer.\
                    Consider re-compiling the model with environment variable MIOPEN_FIND_ENFORCE=3 to re-tune the model."
                              << std::endl;
                }

                auto v          = ctx.to_value();
                auto v_gfx_arch = v.at("gfx_arch").to<std::string>();

                auto v_cu_count        = v.at("cu_count");
                std::size_t n_cu_count = v_cu_count.without_key().to<std::size_t>();

                std::string v_miopen_version = v.at("miopen_version").to<std::string>();
                auto current_device          = ctx.get_current_device();
                std::string current_gfx_arch = current_device.get_device_name();
                std::size_t current_cu_count = current_device.get_cu_count();
                if(n_cu_count != current_cu_count or v_gfx_arch != current_gfx_arch)
                {
                    MIGRAPHX_THROW(
                        "MIGraphX model was compiled for gfx_arch: " + v_gfx_arch +
                        " with number of CUs=" + std::to_string(n_cu_count) +
                        ", but current device has gfx_arch: " + current_gfx_arch +
                        " with number of CUs=" + std::to_string(current_cu_count) +
                        ", performance may suffer. Consider re-compiling the model with "
                        "environment variable MIOPEN_FIND_ENFORCE=3 to re-tune the model.");
                }
            }
        }
#else
        // Use immediate mode API
        {
            set_conv_descriptor();
            if(solution_id == 0)
            {
                // Check that workspace hasn't changed
                auto size = inputs.at(2).bytes();
                auto ws   = find(ctx, output_shape, inputs);
                if(ws.bytes() > size)
                    MIGRAPHX_THROW("MIOpen " + op.name() +
                                   ": workspace has changed during finalization.");
            }

            auto x_desc = make_tensor(reshape_if_1d(inputs[0]), int8_x4_format);
            auto w_desc = make_tensor(reshape_if_1d(inputs[1]), int8_x4_format);
            auto y_desc = make_tensor(reshape_if_1d(output_shape));

            auto status = miopenConvolutionForwardCompileSolution(ctx.get_stream().get_miopen(),
                                                                  w_desc.get(),
                                                                  x_desc.get(),
                                                                  cd.get(),
                                                                  y_desc.get(),
                                                                  solution_id);
            if(status != miopenStatusSuccess)
                MIGRAPHX_THROW("MIOpen Convolution: compile solution failed");
        }
#endif
    }

    inline std::ptrdiff_t output_alias(const std::vector<shape>& shapes) const
    {
        return shapes.size() - 1;
    }

    inline shape pack_int8_shape(const shape& s) const
    {
        if(s.type() != shape::int8_type)
        {
            return s;
        }

        auto lens    = s.lens();
        auto strides = s.strides();
        lens[1]      = (lens[1] + 3) / 4 * 4;
        strides[0]   = strides[1] * lens[1];

        return {s.type(), lens, strides};
    }
};
} // namespace gpu
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif
