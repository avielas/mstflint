/*
 * Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. ALL RIGHTS RESERVED.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * doca_contract/probe.c -- a stand-in for DOCA's libs/doca_mgmt.
 *
 * This is not a functional test of the SDK; the other suites do that. It
 * exists to exercise the *integration contract* our only external consumer
 * relies on, which every other test in this tree bypasses:
 *
 *   1. plain C.  DOCA compiles six .c files against the extern "C" surface and
 *      never touches the C++ class API, so it is immune to the std::string ABI
 *      hazard that the C++ suites can hit. Keep this file .c.
 *   2. <mft_sdk/mft_sdk.h>.  The historical MFT include spelling, resolved
 *      purely from the .pc Cflags. This is why the headers install nested at
 *      $(includedir)/mstflint/sdk/mft_sdk/ -- flatten that and every DOCA
 *      #include breaks.
 *   3. mstGetDeviceHandleByFwctlDeviceName.  DOCA's ONLY device-open entry
 *      point, and the one API that exists because of this integration. No
 *      other suite in this tree links it.
 *   4. no rpath.  The Makefile links this binary without -rpath on purpose,
 *      because DOCA sets rpath empty for all its libraries. The loader must
 *      find libmstflint_sdk.so through the ld.so.conf.d snippet alone. Every
 *      other binary here bakes in an rpath and would keep working even if that
 *      snippet regressed -- which is exactly how doca-dms went red in UBS
 *      while all of our tests stayed green.
 *
 * Exit status is about the *contract*, not the device:
 *   0  the SDK was found, loaded and entered  (any MstStatus is acceptable)
 *   1  the SDK was reached but behaved structurally wrong
 * A loader failure never reaches main() at all -- the shell reports 127 and
 * check_contract.sh turns that into an explicit diagnosis.
 */

#include <stdio.h>
#include <string.h>
#include <mft_sdk/mft_sdk.h>

int main(int argc, char** argv)
{
    const char* fwctlName = (argc > 1) ? argv[1] : "fwctl0";
    MstDevice   dev = NULL;
    MstStatus   st;

    printf("include  <mft_sdk/mft_sdk.h>  resolved\n");

    st = mstGetDeviceHandleByFwctlDeviceName(&dev, fwctlName);
    printf("mstGetDeviceHandleByFwctlDeviceName(\"%s\") -> %d\n", fwctlName, (int)st);

    /* Both error-string entry points are part of DOCA's 23 symbols and are
     * called on every failure path in doca_mgmt.c, so exercise them here
     * rather than only asserting they exist in the symbol table.
     * mstGetInitErrorString() is the one that carries the open failure;
     * mstGetLastErrorString() takes a device and is only meaningful once we
     * hold one, which is how doca_mgmt.c uses it. */
    {
        const char* initErr = mstGetInitErrorString();
        printf("mstGetInitErrorString()   -> %s\n", initErr ? initErr : "(null)");
    }

    if (st == MST_SUCCESS)
    {
        const char* lastErr;

        if (dev == NULL)
        {
            fprintf(stderr, "FAIL: MST_SUCCESS but the handle is NULL\n");
            return 1;
        }
        lastErr = mstGetLastErrorString(dev);
        printf("mstGetLastErrorString()   -> %s\n", lastErr ? lastErr : "(null)");
        printf("mstReleaseDeviceHandle()  -> %d\n", (int)mstReleaseDeviceHandle(dev));
        printf("RESULT: device opened -- full contract exercised\n");
        return 0;
    }

    /* No fwctl device on this machine is the common case on lab hosts (the
     * kernel modules fwctl.ko / mlx5_fwctl.ko are often not loaded) and on any
     * non-root run. We still got into SDK code and back, which is what the
     * link-and-load half of the contract is about. */
    printf("RESULT: SDK entered and returned status %d"
           " -- link/load contract OK, device open not exercised\n", (int)st);
    return 0;
}
