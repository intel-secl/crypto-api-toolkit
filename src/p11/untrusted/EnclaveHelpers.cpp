/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "EnclaveHelpers.h"

// Globals with file scope.
namespace P11Crypto
{
    sgx_enclave_id_t    EnclaveHelpers::mEnclaveInvalidId       = 0;
    volatile long       EnclaveHelpers::mSgxEnclaveLoadedCount  = 0;
    sgx_enclave_id_t    EnclaveHelpers::mSgxEnclaveId           = 0;
    std::string         enclaveFileName                         = (("NONE" == installationPath)? defaultLibraryPath : libraryDirectory) + "libp11SgxEnclave.signed.so";

    //---------------------------------------------------------------------------------------------
    EnclaveHelpers::EnclaveHelpers()
    {

    }

    //---------------------------------------------------------------------------------------------
    sgx_status_t EnclaveHelpers::loadSgxEnclave()
    {
        sgx_status_t     sgxStatus    = sgx_status_t::SGX_ERROR_UNEXPECTED;
        sgx_enclave_id_t sgxEnclaveId = mEnclaveInvalidId;

        if (isSgxEnclaveLoaded())
        {
            // The Intel SGX enclave is already loaded so return success.
            __sync_add_and_fetch(&mSgxEnclaveLoadedCount, 1);
            return SGX_SUCCESS;
        }

        // There is no need to handle SGX_ERROR_ENCLAVE_LOST here because
        // for sure this is the first new instance of enclave creation.
        sgxStatus = sgx_create_enclave(enclaveFileName.data(),
                                       SGX_DEBUG_FLAG,
                                       NULL,
                                       NULL,
                                       &sgxEnclaveId,
                                       NULL);

#if RELEASE_WHITELISTED_ENCLAVE
        if (sgx_status_t::SGX_ERROR_SERVICE_INVALID_PRIVILEGE == sgxStatus)
        {
            // If error indicates that enclave verification fails due to cert not being
            // in white list, register the embedded white list cert binary and retry
            // loading the enclave. Registration is a one-time operation.
            // Please refer to sgx_register_wl_cert_chain
            // https://software.intel.com/en-us/sgx-sdk-dev-reference-sgx-register-wl-cert-chain
            // After registration call sgx_create_enclave
        }
#endif
        // Save the SGX enclave ID for later.
        if (sgx_status_t::SGX_SUCCESS == sgxStatus)
        {
            setSgxEnclaveId(sgxEnclaveId);
            __sync_add_and_fetch(&mSgxEnclaveLoadedCount, 1);
        }
        else
        {
            __sync_lock_test_and_set(&mSgxEnclaveLoadedCount, 0);
            sgx_destroy_enclave(sgxEnclaveId);
            setSgxEnclaveId(mEnclaveInvalidId);
        }

        return sgxStatus;
    } // end loadSgxEnclave()

    //---------------------------------------------------------------------------------------------
    sgx_status_t EnclaveHelpers::unloadSgxEnclave()
    {
        sgx_status_t sgxStatus      = SGX_ERROR_UNEXPECTED;

        do
        {
            if (false == isSgxEnclaveLoaded())
            {
                sgxStatus = sgx_status_t::SGX_SUCCESS;
                break;
            }

            __sync_sub_and_fetch(&mSgxEnclaveLoadedCount, 1);

            /*
            (void)deinitCryptoEnclave(getSgxEnclaveId(),
                                      reinterpret_cast<int32_t*>(&enclaveStatus));

            */
            // The Intel SGX enclave is already
            // in use so return success.
            if (mSgxEnclaveLoadedCount > 0)
            {
                sgxStatus = SGX_SUCCESS;
                break;
            }

            sgxStatus = sgx_destroy_enclave(getSgxEnclaveId());

            if (sgx_status_t::SGX_SUCCESS == sgxStatus)
            {
                setSgxEnclaveId(mEnclaveInvalidId);
                __sync_lock_test_and_set(&mSgxEnclaveLoadedCount, 0);
            }

        } while (false);

        return sgxStatus;
    } // unloadSgxEnclave()
}
