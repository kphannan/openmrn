/** \copyright
 * Copyright (c) 2016, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file simplelink.h
 * Compatibility header for including before the CC3220-sdk.
 *
 * @author Balazs Racz
 * @date 18 Mar 2017
 */

#ifndef _FREEERTOS_DRIVERS_TI_CC3200_COMPAT_SIMPLELINK_H_
#define _FREEERTOS_DRIVERS_TI_CC3200_COMPAT_SIMPLELINK_H_

#ifndef __USER_H__
#define __USER_H__
#endif

#ifdef SL_API_V2
#include "CC3220/user.h"
#include "sl_compat.h"
#include "ti/drivers/net/wifi/simplelink.h"
#else
#include "simplelink_v1.h"
#include "CC3200/user.h"
#include "simplelink/include/simplelink.h"
#endif

#endif // _FREEERTOS_DRIVERS_TI_CC3200_COMPAT_SIMPLELINK_H_
