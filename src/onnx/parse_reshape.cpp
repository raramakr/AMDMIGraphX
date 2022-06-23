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
#include <migraphx/onnx/op_parser.hpp>
#include <migraphx/onnx/checks.hpp>
#include <migraphx/instruction.hpp>
#include <migraphx/ranges.hpp>
#include <migraphx/make_op.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace onnx {

struct parse_reshape : op_parser<parse_reshape>
{
    std::vector<op_desc> operators() const { return {{"Reshape"}}; }

    instruction_ref parse(const op_desc& /*opd*/,
                          const onnx_parser& parser,
                          onnx_parser::node_info info,
                          std::vector<instruction_ref> args) const
    {
        std::vector<int64_t> dims;
        if(args.size() == 1)
        {
            literal s = parser.parse_value(info.attributes.at("shape"));
            s.visit([&](auto v) { copy(v, std::back_inserter(dims)); });
        }
        if(args.size() == 2)
        {
            auto s = args[1]->eval();
            check_arg_empty(s, "Reshape: dynamic shape is not supported");
            s.visit([&](auto v) { copy(v, std::back_inserter(dims)); });
        }

        std::vector<size_t> orig_dims = args[0]->get_shape().lens();

        auto dims_acc = std::accumulate(dims.begin(), dims.end(), 1, std::multiplies<int64_t>());
        auto orig_dims_acc =
            std::accumulate(orig_dims.begin(), orig_dims.end(), 1, std::multiplies<int64_t>());

        if(std::abs(dims_acc) == orig_dims_acc)
            return info.add_instruction(make_op("reshape", {{"dims", dims}}),
                                        info.make_contiguous(args[0]));
        if(parser.default_dim_value * std::abs(dims_acc) == orig_dims_acc)
        {
            dims[0] *= parser.default_dim_value;
            return info.add_instruction(make_op("reshape", {{"dims", dims}}),
                                        info.make_contiguous(args[0]));
        }
        MIGRAPHX_THROW("Reshape: incorrect number of elements: " + std::to_string(dims_acc) +
                       " versus " + std::to_string(orig_dims_acc));
    }
};

} // namespace onnx
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx
