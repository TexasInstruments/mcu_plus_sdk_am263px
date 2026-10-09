/*
 *  Copyright (C) 2026 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ICSS_EMAC_VER_H_
#define ICSS_EMAC_VER_H_

/**
 *  \file icss_emac_ver.h
 *
 *  \brief ICSS-EMAC LLD version information.
 *
 *  Version encoding: 32-bit value MAJOR.MINOR.PATCH.BUILD
 *
 *  When to increment each component:
 *    MAJOR - EMAC LLD redesign or drop-in support removal (API break).
 *            Example: 01.00.00.00 -> 02.00.00.00
 *    MINOR - Introduction of a new protocol or feature that does not break
 *            any existing functionality.
 *            Example: 01.03.00.00 -> 01.04.00.00
 *    PATCH - Targeted bug-fix release; no changes in icss_emac.h for API compatibility
 *            Example: 01.04.00.00 -> 01.04.01.00
 *    BUILD - Auto-incremented identifier for every internal CI/CD build.
 *            Example: 01.04.01.00 -> 01.04.01.01
 */

#include <kernel/dpl/DebugP.h>

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

#define ICSS_EMAC_LLD_VERSION_MAJOR  (0x01U)   /*!< LLD major version */
#define ICSS_EMAC_LLD_VERSION_MINOR  (0x00U)   /*!< LLD minor version */
#define ICSS_EMAC_LLD_VERSION_PATCH  (0x01U)   /*!< LLD patch version */
#define ICSS_EMAC_LLD_VERSION_BUILD  (0x01U)   /*!< LLD build number  */

/** Composite 32-bit version ID: bits[31:24]=MAJOR, [23:16]=MINOR, [15:8]=PATCH, [7:0]=BUILD */
#define ICSS_EMAC_LLD_VERSION_ID_BUILD(a, b, c, d)  (((uint32_t)(a) << 24U) | \
                                                    ((uint32_t)(b) << 16U) | \
                                                    ((uint32_t)(c) <<  8U) | \
                                                    ((uint32_t)(d) <<  0U))

/** Current version of iccs_emac */                                                    
#define ICSS_EMAC_LLD_VERSION_ID \
    ICSS_EMAC_LLD_VERSION_ID_BUILD( \
        ICSS_EMAC_LLD_VERSION_MAJOR, \
        ICSS_EMAC_LLD_VERSION_MINOR, \
        ICSS_EMAC_LLD_VERSION_PATCH, \
        ICSS_EMAC_LLD_VERSION_BUILD)

/** Macro used to check if library and header have exact same version.
 *  This enforces recompilation of all involved libraries on every change.
 */
#define ICSS_EMAC_ASSERT_LIBVER_IDENTICAL DebugP_assert((ICSS_EMAC_getVersion() ^ ICSS_EMAC_LLD_VERSION_ID) == 0)

/** Macro used to check if API of header and running lib is compatible
 *  This allows compile of icss_emac lib only if there is no API change
 */
#define ICSS_EMAC_ASSERT_LIBVER_API_COMPATIBLE DebugP_assert(((ICSS_EMAC_getVersion() ^ ICSS_EMAC_LLD_VERSION_ID) & 0xFFFF0000) == 0)

/** Macro used to check if minimal version is used  for header and lib */
#define ICSS_EMAC_ASSERT_VER_MINIMAL(a, b, c, d) DebugP_assert(ICSS_EMAC_getVersion() >= ICSS_EMAC_LLD_VERSION_ID_BUILD((a), (b), (c), (d)) \
                                                    && ICSS_EMAC_LLD_VERSION_ID >= ICSS_EMAC_LLD_VERSION_ID_BUILD((a), (b), (c), (d)))
/** Macro used to check if exact version is used for header and lib */
#define ICSS_EMAC_ASSERT_VER_EXACT(a, b, c, d) DebugP_assert(ICSS_EMAC_getVersion() == ICSS_EMAC_LLD_VERSION_ID_BUILD((a), (b), (c), (d)) \
                                                    && ICSS_EMAC_LLD_VERSION_ID == ICSS_EMAC_LLD_VERSION_ID_BUILD((a), (b), (c), (d)))

/** Macro used to check if correct minor version is used for header and lib */
#define ICSS_EMAC_ASSERT_VER_MINOR(a, b) DebugP_assert((ICSS_EMAC_getVersion() & 0xFFFF0000) == ICSS_EMAC_LLD_VERSION_ID_BUILD((a), (b), 0, 0) \
                                                    && (ICSS_EMAC_LLD_VERSION_ID & 0xFFFF0000) == ICSS_EMAC_LLD_VERSION_ID_BUILD((a), (b), 0, 0))

#endif /* ICSS_EMAC_VER_H_ */
