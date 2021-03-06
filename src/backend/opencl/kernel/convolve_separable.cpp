/*******************************************************
 * Copyright (c) 2014, ArrayFire
 * All rights reserved.
 *
 * This file is distributed under 3-clause BSD license.
 * The complete license agreement can be obtained at:
 * http://arrayfire.com/licenses/BSD-3-Clause
 ********************************************************/

#include <kernel_headers/convolve_separable.hpp>

#include <Param.hpp>
#include <common/dispatch.hpp>
#include <common/kernel_cache.hpp>
#include <debug_opencl.hpp>
#include <kernel/names.hpp>
#include <kernel_headers/ops.hpp>
#include <memory.hpp>
#include <traits.hpp>

#include <string>
#include <vector>

namespace opencl {
namespace kernel {

template<typename T, typename accType, int conv_dim, bool expand>
void convSep(Param out, const Param signal, const Param filter) {
    constexpr int THREADS_X = 16;
    constexpr int THREADS_Y = 16;
    constexpr bool IsComplex =
        std::is_same<T, cfloat>::value || std::is_same<T, cdouble>::value;

    static const std::string src1(ops_cl, ops_cl_len);
    static const std::string src2(convolve_separable_cl,
                                  convolve_separable_cl_len);

    const int fLen       = filter.info.dims[0] * filter.info.dims[1];
    const size_t C0_SIZE = (THREADS_X + 2 * (fLen - 1)) * THREADS_Y;
    const size_t C1_SIZE = (THREADS_Y + 2 * (fLen - 1)) * THREADS_X;
    size_t locSize       = (conv_dim == 0 ? C0_SIZE : C1_SIZE);

    std::vector<TemplateArg> tmpltArgs = {
        TemplateTypename<T>(), TemplateTypename<accType>(),
        TemplateArg(conv_dim), TemplateArg(expand),
        TemplateArg(fLen),
    };
    std::vector<std::string> compileOpts = {
        DefineKeyValue(T, dtype_traits<T>::getName()),
        DefineKeyValue(Ti, dtype_traits<T>::getName()),
        DefineKeyValue(To, dtype_traits<accType>::getName()),
        DefineKeyValue(accType, dtype_traits<accType>::getName()),
        DefineKeyValue(CONV_DIM, conv_dim),
        DefineKeyValue(EXPAND, (expand ? 1 : 0)),
        DefineKeyValue(FLEN, fLen),
        DefineKeyFromStr(binOpName<af_mul_t>()),
        DefineKeyValue(IS_CPLX, (IsComplex ? 1 : 0)),
        DefineKeyValue(LOCAL_MEM_SIZE, locSize),
    };
    compileOpts.emplace_back(getTypeBuildDefinition<T>());

    auto conv =
        common::findKernel("convolve", {src1, src2}, tmpltArgs, compileOpts);

    cl::NDRange local(THREADS_X, THREADS_Y);

    int blk_x = divup(out.info.dims[0], THREADS_X);
    int blk_y = divup(out.info.dims[1], THREADS_Y);

    cl::NDRange global(blk_x * signal.info.dims[2] * THREADS_X,
                       blk_y * signal.info.dims[3] * THREADS_Y);

    cl::Buffer *mBuff = bufferAlloc(fLen * sizeof(accType));
    // FIX ME: if the filter array is strided, direct might cause issues
    getQueue().enqueueCopyBuffer(*filter.data, *mBuff, 0, 0,
                                 fLen * sizeof(accType));

    conv(cl::EnqueueArgs(getQueue(), global, local), *out.data, out.info,
         *signal.data, signal.info, *mBuff, blk_x, blk_y);
    bufferFree(mBuff);
}

#define INSTANTIATE(T, accT)                                             \
    template void convSep<T, accT, 0, true>(Param out, const Param sig,  \
                                            const Param filt);           \
    template void convSep<T, accT, 1, true>(Param out, const Param sig,  \
                                            const Param filt);           \
    template void convSep<T, accT, 0, false>(Param out, const Param sig, \
                                             const Param filt);          \
    template void convSep<T, accT, 1, false>(Param out, const Param sig, \
                                             const Param filt);

INSTANTIATE(cdouble, cdouble)
INSTANTIATE(cfloat, cfloat)
INSTANTIATE(double, double)
INSTANTIATE(float, float)
INSTANTIATE(uint, float)
INSTANTIATE(int, float)
INSTANTIATE(uchar, float)
INSTANTIATE(char, float)
INSTANTIATE(ushort, float)
INSTANTIATE(short, float)
INSTANTIATE(uintl, float)
INSTANTIATE(intl, float)

}  // namespace kernel
}  // namespace opencl
