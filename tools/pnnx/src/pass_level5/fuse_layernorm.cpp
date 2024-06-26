// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "fuse_layernorm.h"

#include "pass_level2.h"

#include <math.h>
#include <string.h>

#include <torch/csrc/api/include/torch/version.h>

namespace pnnx {

class fuse_layernorm_pass : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
8 7
pnnx.Input              input       0 1 input #input=(1,%c,?,?)f32
pnnx.Attribute          op_0        0 1 weight @data #weight=(%c,1,1)f32
pnnx.Attribute          op_1        0 1 bias @data #bias=(%c,1,1)f32
torch.mean              op_2        1 1 input mean dim=(1) keepdim=True
pnnx.Expression         op_3        2 1 input mean 2 expr=pow(sub(@0,@1),2)
torch.mean              op_4        1 1 2 var dim=(1) keepdim=True
pnnx.Expression         op_5        5 1 weight input mean var bias out expr=add(mul(@0,div(sub(@1,@2),sqrt(add(@3,%eps)))),@4)
pnnx.Output             output      1 0 out
)PNNXIR";
    }

    const char* replace_pattern_graph() const
    {
        return R"PNNXIR(7767517
5 4
pnnx.Input              input       0 1 input
Tensor.permute          op_0        1 1 input a dims=(0,2,3,1)
nn.LayerNorm            op_1        1 1 a b elementwise_affine=True eps=%eps normalized_shape=(%c) @weight=%op_0.data @bias=%op_1.data
Tensor.permute          op_2        1 1 b out dims=(0,3,1,2)
pnnx.Output             output      1 0 out
)PNNXIR";
    }

    void write(const std::map<std::string, Operator*>& ops, const std::map<std::string, Parameter>& captured_params, const std::map<std::string, Attribute>& captured_attrs) const
    {
        GraphRewriterPass::write(ops, captured_params, captured_attrs);

        // fix weight bias shape from (c,1,1) to (c)
        const int c = captured_params.at("c").i;
        Operator* op_1 = ops.at("op_1");
        op_1->attrs["weight"].shape = {c};
        op_1->attrs["bias"].shape = {c};
    }
};

class fuse_layernorm_pass_1 : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        // clang-format off
        // *INDENT-OFF*

        return R"PNNXIR(7767517
9 8
pnnx.Input              input       0 1 input #input=(1,?,%c)f32
pnnx.Attribute          op_0        0 1 weight @data #weight=(%c)f32
pnnx.Attribute          op_1        0 1 bias @data #bias=(%c)f32
torch.mean              op_2        1 1 input mean dim=(-1) keepdim=True
pnnx.Expression         op_3        2 1 input mean 173 expr=sub(@0,@1)
pnnx.Expression         op_4        1 1 173 174 expr=pow(@0,2.000000e+00)
torch.mean              op_5        1 1 174 var dim=(-1) keepdim=True
pnnx.Expression         op_6        4 1 173 var weight bias out expr=add(mul(div(@0,sqrt(add(@1,%eps))),@2),@3)
pnnx.Output             output      1 0 out
)PNNXIR";

        // *INDENT-ON*
        // clang-format on
    }

    const char* replace_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 2
pnnx.Input              input       0 1 input
nn.LayerNorm            ln          1 1 input out elementwise_affine=True eps=%eps normalized_shape=(%c) @weight=%op_0.data @bias=%op_1.data
pnnx.Output             output      1 0 out
)PNNXIR";
    }
};

class fuse_layernorm_pass_1_1 : public fuse_layernorm_pass_1
{
public:
    const char* match_pattern_graph() const
    {
        // clang-format off
        // *INDENT-OFF*
        return R"PNNXIR(7767517
9 8
pnnx.Input              input       0 1 input #input=(1,?,%c)f32
pnnx.Attribute          op_0        0 1 weight @data #weight=(%c)f32
pnnx.Attribute          op_1        0 1 bias @data #bias=(%c)f32
torch.mean              op_2        1 1 input mean dim=(2) keepdim=True #input=(1,?,%c)f32
pnnx.Expression         op_3        2 1 input mean 75 expr=sub(@0,@1)
pnnx.Expression         op_4        1 1 75 76 expr=pow(@0,2.000000e+00)
torch.mean              op_5        1 1 76 var dim=(2) keepdim=True
pnnx.Expression         op_6        4 1 75 var weight bias out expr=add(mul(div(@0,sqrt(add(@1,%eps))),@2),@3)
pnnx.Output             output      1 0 out
)PNNXIR";

        // *INDENT-ON*
        // clang-format on
    }
};

class fuse_layernorm_pass_2 : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        // clang-format off
        // *INDENT-OFF*

        return R"PNNXIR(7767517
7 6
pnnx.Input              input       0 1 input #input=(1,?,%c)f32
torch.mean              op_0        1 1 input mean dim=(-1) keepdim=True
pnnx.Expression         op_1        2 1 input mean 25 expr=sub(@0,@1)
pnnx.Expression         op_2        1 1 25 26 expr=pow(@0,2.000000e+00)
torch.mean              op_3        1 1 26 var dim=(-1) keepdim=True
pnnx.Expression         op_4        2 1 25 var out expr=div(@0,sqrt(add(@1,%eps)))
pnnx.Output             output      1 0 out
)PNNXIR";

        // *INDENT-ON*
        // clang-format on
    }

    const char* replace_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 2
pnnx.Input              input       0 1 input
nn.LayerNorm            ln          1 1 input out elementwise_affine=False eps=%eps normalized_shape=(%c)
pnnx.Output             output      1 0 out
)PNNXIR";
    }
};

class fuse_layernorm_pass_2_1 : public fuse_layernorm_pass_2
{
public:
    const char* match_pattern_graph() const
    {
        // clang-format off
        // *INDENT-OFF*

        return R"PNNXIR(7767517
7 6
pnnx.Input              input       0 1 input #input=(1,?,?,%c)f32
torch.mean              op_0        1 1 input mean dim=(-1) keepdim=True
pnnx.Expression         op_1        2 1 input mean 32 expr=sub(@0,@1)
pnnx.Expression         op_2        1 1 32 33 expr=pow(@0,2.000000e+00)
torch.mean              op_3        1 1 33 var dim=(-1) keepdim=True
pnnx.Expression         op_4        2 1 32 var out expr=div(@0,sqrt(add(@1,%eps)))
pnnx.Output             output      1 0 out
)PNNXIR";

        // *INDENT-ON*
        // clang-format on
    }
};

void fuse_layernorm(Graph& graph)
{
    fuse_layernorm_pass a;
    fuse_layernorm_pass_1 b;
    fuse_layernorm_pass_1_1 b1;
    fuse_layernorm_pass_2 c;
    fuse_layernorm_pass_2_1 c1;
    int opindex = 0;

    pnnx_graph_rewrite(graph, &a, opindex);
    pnnx_graph_rewrite(graph, &b, opindex);
    pnnx_graph_rewrite(graph, &b1, opindex);
    pnnx_graph_rewrite(graph, &c, opindex);
    pnnx_graph_rewrite(graph, &c1, opindex);
}

} // namespace pnnx
