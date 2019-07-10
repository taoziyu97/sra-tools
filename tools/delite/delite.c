/*==============================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include <kapp/main.h> /* KMain */
#include <kapp/args.h> /* KMain */

#include <klib/text.h>
#include <klib/printf.h>
#include <klib/out.h>       /* OUTMSG, KOutHandlerSetStdOut */
#include <klib/status.h>    /* KStsHandlerSetStdErr */
#include <klib/debug.h>     /* KDbgHandlerSetStdErr */
#include <klib/refcount.h>
#include <klib/rc.h>
#include <klib/log.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <kfg/config.h>

#include "delite.h"
#include "delite_k.h"

#include <sysalloc.h>
#include <stdio.h>
#include <string.h>

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Kinda legendary intentions
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/******************************************************************************/

/*)))
  \\\   Celebrity is here ...
  (((*/

const char * ProgNm = "sra-delite";  /* Application program name */

static rc_t __porseAndHandle ( int Arc, char * ArgV [], struct DeLiteParams * Params );
static rc_t __runKar ( struct DeLiteParams * Params );

rc_t CC
KMain ( int ArgC, char * ArgV [] )
{
    rc_t RCt;
    struct DeLiteParams DLP;

    RCt = 0;

    RCt = __porseAndHandle ( ArgC, ArgV, & DLP );
    if ( RCt == 0 ) {
            /*  Something very special
             */
        if ( DLP . _output_stdout ) {
            KOutHandlerSetStdErr();
            KStsHandlerSetStdErr();
            KLogHandlerSetStdErr();
            KDbgHandlerSetStdErr();
        }

        RCt = __runKar ( & DLP );
    }

    DeLiteParamsWhack ( & DLP );

    return RCt;
}

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Dansing from here and till the fence
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Lite
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Arguments
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
#define OPT_OUTPUT      "output"
#define ALS_OUTPUT      NULL
#define PRM_OUTPUT      NULL
#define STDOUT_OUTPUT   "--"

#define OPT_SCHEMA      "schema"
#define ALS_SCHEMA      "S"
#define PRM_SCHEMA      NULL

#define OPT_TRANSF      "transf"
#define ALS_TRANSF      NULL
#define PRM_TRANSF      NULL

#define OPT_NOEDIT      "noedit"
#define ALS_NOEDIT      NULL
#define PRM_NOEDIT      NULL

#define OPT_UPDATE      "update"
#define ALS_UPDATE      NULL
#define PRM_UPDATE      NULL

#define OPT_DELITE      "delite"
#define ALS_DELITE      NULL
#define PRM_DELITE      NULL

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Params
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t
DeLiteParamsSetProgram (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    const char * Value;
    rc_t RCt;

    Value = NULL;
    RCt = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsProgram ( TheArgs, NULL, & Value );
    if ( RCt == 0 ) {
        RCt = copyStringSayNothingRelax ( & Params -> _program, Value );
    }

    return RCt;
}   /* DeLiteParamsSetProgram () */

static
rc_t
DeLiteParamsSetAccession (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    uint32_t ParamCount;
    const char * Value;
    rc_t RCt;

    ParamCount = 0;
    Value = NULL;
    RCt = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsParamCount ( TheArgs, & ParamCount );
    if ( RCt == 0 ) {
        if ( ParamCount == 1 ) {
            RCt = ArgsParamValue (
                                TheArgs,
                                0,
                                ( const void ** ) & Value
                                );
            if ( RCt == 0 ) {
                RCt = copyStringSayNothingRelax (
                                                & Params -> _accession,
                                                Value
                                                );
            }
        }
        else {
            RCt = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInsufficient );
        }
    }

    return RCt;
}   /* DeLiteParamsSetAccession () */

static
rc_t
DeLiteParamsSetOutput (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    rc_t RCt;
    const char * Value;
    uint32_t OptCount;

    RCt = 0;
    Value = NULL;
    OptCount = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsOptionCount ( TheArgs, OPT_OUTPUT, & OptCount );
    if ( RCt == 0 ) {
        if ( OptCount != 0 ) {
            RCt = ArgsOptionValue (
                                    TheArgs,
                                    OPT_OUTPUT,
                                    0,
                                    ( const void ** ) & Value
                                    ); 
            if ( RCt == 0 ) {
                RCt = copyStringSayNothingRelax (
                                                & Params -> _output,
                                                Value
                                                );
            }
        }
        else {
            return RC ( rcApp, rcArgv, rcParsing, rcParam, rcInsufficient );
        }
    }

    if ( RCt == 0 ) {
        if ( strcmp ( Params -> _output, STDOUT_OUTPUT ) == 0 ) {
            Params -> _output_stdout = true;
        }
    }

    return RCt;
}   /* DeLiteParamsSetOutput () */

static
rc_t
DeLiteParamsSetSingleArgParam (
                        const char ** Param,
                        const char * ArgName,
                        const struct Args * TheArgs
)
{
    rc_t RCt;
    const char * Value;
    uint32_t OptCount;

    RCt = 0;
    Value = NULL;
    OptCount = 0;

    if ( Param != NULL ) {
        * Param = NULL;
    }

    if ( Param == NULL || ArgName == NULL || TheArgs == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    RCt = ArgsOptionCount ( TheArgs, ArgName, & OptCount );
    if ( RCt == 0 ) {
        if ( OptCount != 0 ) {
            RCt = ArgsOptionValue (
                                    TheArgs,
                                    ArgName,
                                    0,
                                    ( const void ** ) & Value
                                    ); 
            if ( RCt == 0 ) {
                RCt = copyStringSayNothingRelax ( Param, Value );
            }
        }
    }

    return RCt;
}   /* DeLiteParamsSetSingleArgParam () */

static
rc_t
DeleteParamsSetBooleanParam (
                            bool * Param,
                            const char * ParamName,
                            const struct Args * TheArgs,
                            bool DefaultValue
)
{
    uint32_t OptCount = 0;

    if ( Param != NULL ) {
        * Param = DefaultValue;
    }

    if ( Param == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    if ( ParamName == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcName, rcNull );
    }

    if ( ArgsOptionCount ( TheArgs, ParamName, & OptCount ) == 0 ) {
        if ( OptCount != 0 ) {
            * Param = true;
        }
    }

    return 0;
}   /* DeleteParamsSetBooleanParam () */

static
rc_t
DeLiteParamsSetNoedit (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    return DeleteParamsSetBooleanParam (
                                        & ( Params -> _noedit ),
                                        OPT_NOEDIT,
                                        TheArgs,
                                        false
                                        );
}   /* DeLiteParamsSetNoedit () */

static
rc_t
DeLiteParamsSetUpdate (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    return DeleteParamsSetBooleanParam (
                                        & ( Params -> _update ),
                                        OPT_UPDATE,
                                        TheArgs,
                                        false
                                        );
}   /* DeLiteParamsSetUpdate () */

static
rc_t
DeLiteParamsSetDelite (
                        struct DeLiteParams * Params,
                        const struct Args * TheArgs
)
{
    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    return DeleteParamsSetBooleanParam (
                                        & ( Params -> _delite ),
                                        OPT_DELITE,
                                        TheArgs,
                                        false
                                        );
}   /* DeLiteParamsSetDelite () */

/*)))
  \\\   KApp and Options ...
  (((*/
static const char * UsgAppOutput [] = { "Name of output file. Mandatory, string. If name is \"--\" output will be written to STDOUT", NULL };
static const char * UsgAppSchema [] = { "Path to schema directory to use. Mandatory, string", NULL };
static const char * UsgAppTransf [] = { "Path to the list of schema transformation file. Mandatory, string. File in format <name><tab><old_ver><tab><new_ver>", NULL };
static const char * UsgAppNoedit [] = { "Do not process, just print expected actions. Optional.", NULL };
static const char * UsgAppUpdate [] = { "Update schemas for tables in archive. Optional.", NULL };
static const char * UsgAppDelite [] = { "Delete quality scores in archive. Optional.", NULL };

struct OptDef DeeeOpts [] = {
    {       /* Where we will dump new KAR fiel */
        OPT_OUTPUT,         /* option name */
        ALS_OUTPUT,         /* option alias */
        NULL,               /* help generator */
        UsgAppOutput,       /* help as text is here */
        1,                  /* max amount */
        true,               /* need value */
        true                /* is required, yes, it requires */
    },
    {       /* Option do not edit archive, but repack it */
        OPT_SCHEMA,         /* option name */
        ALS_SCHEMA,         /* option alias */
        NULL,               /* help generator */
        UsgAppSchema,       /* help as text is here */
        1,                  /* max amount */
        true,               /* need value */
        false               /* is required */
    },
    {       /* Option do not edit archive, but repack it */
        OPT_TRANSF,         /* option name */
        ALS_TRANSF,         /* option alias */
        NULL,               /* help generator */
        UsgAppTransf,       /* help as text is here */
        1,                  /* max amount */
        true,               /* need value */
        false               /* is required */
    },
    {       /* Option do not edit archive, but repack it */
        OPT_NOEDIT,         /* option name */
        ALS_NOEDIT,         /* option alias */
        NULL,               /* help generator */
        UsgAppNoedit,       /* help as text is here */
        1,                  /* max amount */
        false,              /* need value */
        false               /* is required */
    },
    {       /* Option to update schemas in archive */
        OPT_UPDATE,         /* option name */
        ALS_UPDATE,         /* option alias */
        NULL,               /* help generator */
        UsgAppUpdate,       /* help as text is here */
        1,                  /* max amount */
        false,              /* need value */
        false               /* is required */
    },
    {       /* Option to delete quality scores in archive */
        OPT_DELITE,         /* option name */
        ALS_DELITE,         /* option alias */
        NULL,               /* help generator */
        UsgAppDelite,       /* help as text is here */
        1,                  /* max amount */
        false,              /* need value */
        false               /* is required */
    }
};  /* OptDef */

const char UsageDefaultName[] = "sra-delite";

rc_t
__porseAndHandle (
                int ArgC,
                char * ArgV [],
                struct DeLiteParams * Params
)
{
    rc_t RCt;
    struct Args * TheArgs;

    RCt = 0;
    TheArgs = NULL;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
    }

    DeLiteParamsInit ( Params );

    RCt = ArgsMakeStandardOptions ( & TheArgs );
    if ( RCt == 0 ) {
        while ( 1 ) {
            RCt = ArgsAddOptionArray (
                                TheArgs, 
                                DeeeOpts,
                                sizeof ( DeeeOpts ) / sizeof ( OptDef )
                                );
            if ( RCt != 0 ) {
                break;
            }

            RCt = ArgsParse ( TheArgs, ArgC, ArgV );
            if ( RCt != 0 ) {
                break;
            }

            if ( ArgC == 1 ) {
                MiniUsage ( TheArgs );
                RCt = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInsufficient );
                break;
            }

            RCt = ArgsHandleStandardOptions ( TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetProgram ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            if ( ArgC == 1 ) {
                break;
            }

            RCt = DeLiteParamsSetAccession ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetOutput ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetSingleArgParam (
                                                & Params -> _schema,
                                                OPT_SCHEMA,
                                                TheArgs
                                                );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetSingleArgParam (
                                                & Params -> _transf,
                                                OPT_TRANSF,
                                                TheArgs
                                                );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetNoedit ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetUpdate ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            RCt = DeLiteParamsSetDelite ( Params, TheArgs );
            if ( RCt != 0 ) {
                break;
            }

            break;
        }

        ArgsWhack ( TheArgs );
    }

    if ( RCt == 0 ) {
        if ( ! Params -> _update && ! Params -> _delite ) {
            KOutMsg ( "One or both parameters should be defined \"%s\" and/or \"%s\"\n", OPT_UPDATE, OPT_DELITE );
            pLogErr (
                    klogErr,
                    RCt,
                    "One or both parameters should be defined $(upd)] and/or [$(dlt)]",
                    "upd=%s,dlt=%s",
                    OPT_UPDATE,
                    OPT_DELITE
                    );

            RCt = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInvalid );
        }
    }

    return RCt;
}   /* __porseAndHande () */

rc_t CC
UsageSummary ( const char * ProgName )
{
    return KOutMsg (
                    "\n"
                    "Usage:\n"
                    "\n"
                    "  %s [options]"
                    " <accession>"
                    "\n"
                    "\n",
                    ProgName
                    );
}   /* UsageSummary () */

rc_t CC
Usage ( const struct Args * TheArgs )
{
    rc_t RCt;
    const char * ProgName;
    const char * FullPath;

    RCt = 0;
    ProgName = NULL;
    FullPath = NULL;

    if ( TheArgs == NULL ) {
        RCt = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    }
    else {
        RCt = ArgsProgram ( TheArgs, & FullPath, & ProgName );
    }

    if ( RCt != 0 ) {
        ProgName = FullPath = UsageDefaultName;
    }

    KOutMsg (
            "\n"
            "This program will remove QUALITY column from SRA archive,"
            " it will change schema, metadata, MD5 summ and repack archive"
            " for all archives except 454 or similar"
            "\n"
            );

    UsageSummary ( ProgName );

    KOutMsg ( "Options:\n" );

    HelpOptionLine (
                ALS_OUTPUT,
                OPT_OUTPUT,
                PRM_OUTPUT,
                UsgAppOutput
                );

    HelpOptionLine (
                ALS_SCHEMA,
                OPT_SCHEMA,
                PRM_SCHEMA,
                UsgAppSchema
                );

    HelpOptionLine (
                ALS_TRANSF,
                OPT_TRANSF,
                PRM_TRANSF,
                UsgAppTransf
                );

    HelpOptionLine (
                ALS_NOEDIT,
                OPT_NOEDIT,
                PRM_NOEDIT,
                UsgAppNoedit
                );

    HelpOptionLine (
                ALS_UPDATE,
                OPT_UPDATE,
                PRM_UPDATE,
                UsgAppUpdate
                );

    HelpOptionLine (
                ALS_DELITE,
                OPT_DELITE,
                PRM_DELITE,
                UsgAppDelite
                );

    KOutMsg ( "\n" );

    KOutMsg ( "Standard Options:\n" );
    HelpOptionsStandard ();
    HelpVersion ( FullPath, KAppVersion () );

    return RCt;
}   /* Usage () */


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Sanctuary
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

static rc_t __readKar ( struct DeLiteParams * Params );

rc_t
__runKar ( struct DeLiteParams * Params )
{
    char BB [ 32 ];
    * BB = 0;

    KOutMsg ( "KAR [%s]\n", Params -> _accession );
    pLogMsg ( klogInfo, "KAR [$(acc)]", "acc=%s", Params -> _accession );

    KOutMsg ( "SCM [%s]\n", Params -> _schema );
    pLogMsg ( klogInfo, "SCM [$(path)]", "path=%s", Params -> _schema );

    if ( Params -> _update ) {
        strcat ( BB, "update" );
    }

    if ( Params -> _delite ) {
        if ( * BB != 0 ) {
            strcat ( BB, " & " );
        }
        strcat ( BB, "delite" );
    }

    KOutMsg ( "ACT [%s]\n", BB );
    pLogMsg ( klogInfo, "ACT [$(act)]", "act=%s", BB );

    if ( Params -> _noedit ) {
        KOutMsg ( "RUN [idle]\n" );
        pLogMsg ( klogInfo, "RUN [idle]", "" );
    }

    return __readKar ( Params );
}   /* __runKar () */

rc_t
__modifyConfig ( struct DeLiteParams * Params )
{
    rc_t RCt = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArc, rcReading, rcParam, rcNull );
    }

    if ( Params -> _schema == NULL ) {
        return RC ( rcApp, rcArc, rcReading, rcParam, rcInvalid );
    }

    RCt = KConfigMake ( & ( Params -> _config ), NULL );
    if ( RCt == 0 ) {
        RCt = KConfigWriteString (
                                Params -> _config,
                                "/vdb/schema/paths",
                                Params -> _schema
                                );

    }

    return RCt;
}   /* __modifyConfig () */

rc_t
__readKar ( struct DeLiteParams * Params )
{
    rc_t RCt = 0;

    if ( Params == NULL ) {
        return RC ( rcApp, rcArc, rcReading, rcParam, rcNull );
    }

    RCt = __modifyConfig ( Params );
    if ( RCt != 0 ) {
        KOutMsg ( "ERR [can not modify config]\n" );
        pLogErr (
                klogErr,
                RCt,
                "ERR [can not modify config]",
                ""
                );
        return RCt;
    }


    RCt = DeLiteResolvePath (
                            ( char ** ) & ( Params -> _accession_path ),
                            Params -> _accession
                            );
    if ( RCt == 0 ) {
        KOutMsg ( "PTH [%s]\n", Params -> _accession_path );
        pLogMsg (
                klogInfo,
                "PTH [$(path)]",
                "path=%s",
                Params -> _accession_path
                );

        RCt = Delite ( Params );
    }
    else {
        KOutMsg ( "NOT FND [%s] RC [%d]\n", Params -> _accession, RCt );
        pLogErr (
                klogErr,
                RCt,
                "NOT FOUND [$(acc)] RC [$(rc)]",
                "acc=%s,rc=%u",
                Params -> _accession,
                RCt
                );
    }

    if ( RCt == 0 ) {
        KOutMsg ( "DONE\n" );
    }

    return RCt;
}   /* __readKar () */
