//--------------------------------------------------------------------------------------------------
/**
 * @file ima.c
 *
 * This file implements functions that can be used to import IMA keys (into the kernel keyring) and
 * verify IMA signatures.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "limit.h"
#include "file.h"
#include "fileDescriptor.h"
#include "sysPaths.h"
#include "ima.h"
#include "file.h"


//--------------------------------------------------------------------------------------------------
/**
 * Max size of IMA command
 */
//--------------------------------------------------------------------------------------------------
#define MAX_CMD_BYTES   4096


//--------------------------------------------------------------------------------------------------
/**
 * Path to evmctl tool. It can be used for producing and verifying IMA signatures. It can be also
 * used to import keys into the kernel keyring.
 */
//--------------------------------------------------------------------------------------------------
#define EVMCTL_PATH   "/usr/bin/evmctl"


//--------------------------------------------------------------------------------------------------
/**
 * Verify a file IMA signature against provided public certificate path
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t VerifyFile
(
    const char * filePath,
    const char * certPath
)
{
    char cmd[MAX_CMD_BYTES] = "";

    snprintf(cmd, sizeof(cmd), "%s ima_verify %s -k %s ", EVMCTL_PATH, filePath, certPath);

    LE_DEBUG("Verify file command: %s", cmd);

    int exitCode = system(cmd);

    if (WIFEXITED(exitCode) && (0 == WEXITSTATUS(exitCode)))
    {
        LE_DEBUG("Verified file: '%s' successfully", filePath);
        return LE_OK;
    }
    else
    {
        LE_ERROR("Failed to verify file '%s' with certificate '%s', exitCode: %d",
                 filePath, certPath, WEXITSTATUS(exitCode));
        return LE_FAULT;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Recursively traverse the directory and verify each file IMA signature against provided public
 * certificate path
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t VerifyDir
(
    const char * dirPath,
    const char * certPath
)
{
    char* pathArrayPtr[] = {(char *)dirPath,
                                    NULL};

    // Open the directory tree to traverse.
    FTS* ftsPtr = fts_open(pathArrayPtr,
                           FTS_PHYSICAL,
                           NULL);

    if (NULL == ftsPtr)
    {
        LE_CRIT("Could not access dir '%s'.  %m.", pathArrayPtr[0]);
        return LE_FAULT;
    }

    // Traverse through the directory tree.
    FTSENT* entPtr;
    while (NULL != (entPtr = fts_read(ftsPtr)))
    {
        LE_DEBUG("Filename: %s, filePath: %s, rootPath: %s, fts_info: %d", entPtr->fts_name,
                                entPtr->fts_accpath,
                                entPtr->fts_path,
                                entPtr->fts_info);

        switch (entPtr->fts_info)
        {
            case FTS_SL:
            case FTS_SLNONE:
                break;

            case FTS_F:
                if (0 != strcmp(entPtr->fts_name, PUB_CERT_NAME ))
                {
                    if (LE_OK != VerifyFile(entPtr->fts_accpath, certPath))
                    {
                        LE_CRIT("Failed to verify file '%s' with public certificate '%s'",
                                entPtr->fts_accpath,
                                certPath);
                        fts_close(ftsPtr);
                        return LE_FAULT;
                    }
                }
                break;
        }

    }

    fts_close(ftsPtr);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Import IMA public certificate to linux keyring. Public certificate must be signed by system
 * private key to import it properly.
 *
 * @return
 *      - LE_OK if imports properly
 *      - LE_FAULT if fails to import
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ImportPublicCert
(
    const char * certPath
)
{

    char cmd[MAX_CMD_BYTES] = "";

    snprintf(cmd, sizeof(cmd), "SECFS=/sys/kernel/security && "
             "grep -q  $SECFS /proc/mounts || mount -n -t securityfs securityfs $SECFS && "
             "ima_id=\"`awk '/\\.ima/ { printf \"%%d\", \"0x\"$1; }' /proc/keys`\" && "
             "%s import %s $ima_id", EVMCTL_PATH, certPath);

    LE_DEBUG("cmd: %s", cmd);

    int exitCode = system(cmd);

    if (WIFEXITED(exitCode) && (0 == WEXITSTATUS(exitCode)))
    {
        LE_DEBUG("Installed certificate: '%s' successfully", certPath);
        return LE_OK;
    }
    else
    {
        LE_ERROR("Failed to import certificate '%s', exitCode: %d",
                 certPath,
                 WEXITSTATUS(exitCode));
        return LE_FAULT;
    }
}
//--------------------------------------------------------------------------------------------------
/**
 * Verify a file IMA signature against provided public certificate path
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
le_result_t ima_VerifyFile
(
    const char * filePath,
    const char * certPath
)
{
    return VerifyFile(filePath, certPath);
}

//--------------------------------------------------------------------------------------------------
/**
 * Recursively traverse the directory and verify each file IMA signature against provided public
 * certificate path
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
le_result_t ima_VerifyDir
(
    const char * dirPath,
    const char * certPath
)
{
    return VerifyDir(dirPath, certPath);
}


//--------------------------------------------------------------------------------------------------
/**
 * Check whether current linux kernel is IMA-enabled or not.
 *
 * @return
 *      - true if IMA is enabled.
 *      - false otherwise.
 */
//--------------------------------------------------------------------------------------------------
bool ima_IsEnabled
(
    void
)
{

    int exitCode = system("(zcat /proc/config.gz | grep CONFIG_IMA=y) &&"
                          " (cat /proc/cmdline | grep \"ima_appraise=enforce\")");

    return WIFEXITED(exitCode) && (0 == WEXITSTATUS(exitCode));
}


//--------------------------------------------------------------------------------------------------
/**
 * Import IMA public certificate to linux keyring. Public certificate must be signed by system
 * private key to import it properly. Only privileged process with right permission and smack
 * label will be able to do that.
 *
 * @return
 *      - LE_OK if imports properly
 *      - LE_FAULT if fails to import
 */
//--------------------------------------------------------------------------------------------------
le_result_t ima_ImportPublicCert
(
    const char * certPath
)
{
    return ImportPublicCert(certPath);
}
