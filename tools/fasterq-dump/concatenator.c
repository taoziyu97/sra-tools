/*===========================================================================
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

#include "concatenator.h"
#include "helper.h"
#include "copy_machine.h"

#include <klib/out.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/progressbar.h>

#include <kproc/thread.h>

#include <kfs/defs.h>
#include <kfs/file.h>
#include <kfs/buffile.h>
#include <kfs/gzip.h>
#include <kfs/bzip.h>

static const char * ct_none_fmt  = "%s";
static const char * ct_gzip_fmt  = "%s.gz";
static const char * ct_bzip2_fmt = "%s.bz2";

static rc_t make_compressed( KDirectory * dir,
                             const char * output_filename,
                             size_t buf_size,
                             compress_t compress,
                             bool force,
                             struct KFile ** dst )
{
    rc_t rc = 0;
    if ( dst != NULL )
        *dst = NULL;
    if ( dir == NULL || dst == NULL || output_filename == NULL)
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        struct KFile * f;
        const char * fmt;
        KCreateMode create_mode = force ? kcmInit : kcmCreate;
        
        switch( compress )
        {
            case ct_none  : fmt = ct_none_fmt; break;
            case ct_gzip  : fmt = ct_gzip_fmt; break;
            case ct_bzip2 : fmt = ct_bzip2_fmt; break;
        }
        rc = KDirectoryCreateFile( dir, &f, false, 0664, create_mode | kcmParents, fmt, output_filename );
        if ( rc != 0 )
            ErrMsg( "concatenator.c make_compressed().KDirectoryCreateFile( '%s' ) -> %R", output_filename, rc );
        else
        {
            if ( buf_size > 0 )
            {
                struct KFile * tmp;
                rc = KBufFileMakeWrite( &tmp, f, false, buf_size );
                if ( rc != 0 )
                    ErrMsg( "concatenator.c make_compressed().KBufFileMakeWrite( '%s' ) -> %R", output_filename, rc );
                else
                {
                    KFileRelease( f );
                    f = tmp;
                }
            }
            if ( rc == 0 && compress != ct_none )
            {
                struct KFile * tmp;
                if ( compress == ct_gzip )
                {
                    rc = KFileMakeGzipForWrite ( &tmp, f );
                    if ( rc != 0 )
                        ErrMsg( "concatenator.c make_compressed().KFileMakeGzipForWrite( '%s' ) -> %R", output_filename, rc );
                }
                else if ( compress == ct_bzip2 )
                {
                    rc = KFileMakeBzip2ForWrite ( &tmp, f );
                    if ( rc != 0 )
                        ErrMsg( "concatenator.c make_compressed().KFileMakeBzip2ForWrite( '%s' ) -> %R", output_filename, rc );
                }
                else
                    rc = RC( rcExe, rcFile, rcPacking, rcMode, rcInvalid );
                
                if ( rc == 0 )
                {
                    KFileRelease( f );
                    f = tmp;
                }
            }
        
            if ( rc == 0 )
                *dst = f;
            else
                KFileRelease( f );
        }
    }
    return rc;
}

/* ---------------------------------------------------------------------------------- */

rc_t execute_concat_compressed( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files,
                    size_t buf_size,
                    struct bg_progress * progress,
                    bool force,
                    bool append,
                    compress_t compress,
                    uint32_t count,
                    uint32_t q_wait_time )
{
    struct KFile * dst;
    rc_t rc =  make_compressed( dir, output_filename, buf_size, compress, force, &dst ); /* above */
    if ( rc == 0 )
    {
        rc = make_a_copy( dir, dst, files, progress, 0, buf_size, 0, q_wait_time ); /* copy_machine.c */
        KFileRelease( dst );
    }
    return rc;
}


/* ---------------------------------------------------------------------------------- */

static rc_t execute_concat_un_compressed_append( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files,
                    size_t buf_size,
                    struct bg_progress * progress,
                    uint32_t count,
                    uint32_t q_wait_time )
{
    uint64_t size_of_existing_file;
    rc_t rc = KDirectoryFileSize ( dir, &size_of_existing_file, "%s", output_filename );
    if ( rc != 0 )
        ErrMsg( "concatenator.c execute_concat_un_compressed_append() KDirectoryFileSize( '%s' ) -> %R",
                output_filename, rc );
    else
    {
        struct KFile * dst;
        rc = KDirectoryOpenFileWrite ( dir, &dst, true, "%s", output_filename );
        if ( rc != 0 )
            ErrMsg( "concatenator.c execute_concat_un_compressed_append() KDirectoryOpenFileWrite( '%s' ) -> %R",
                    output_filename, rc );
        else
        {
            rc = make_a_copy( dir, dst, files, progress, size_of_existing_file, buf_size, 0, q_wait_time ); /* copy_machine.c */
            KFileRelease( dst );
        }
    }
    return rc;
}

static rc_t execute_concat_un_compressed_no_append( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files,
                    size_t buf_size,
                    struct bg_progress * progress,
                    bool force,
                    uint32_t count,
                    uint32_t q_wait_time )
{
    const char * file1;
    rc_t rc = VNameListGet( files, 0, &file1 );
    if ( rc != 0 )
        ErrMsg( "concatenator.c execute_concat_un_compressed() VNameListGet( 0 ) -> %R", rc );
    else
    {
        uint64_t size_file1;
        
        /* we need the size of the first file, as an offset later - if KDirectoryRename() was successful */
        rc = KDirectoryFileSize ( dir, &size_file1, "%s", file1 );
        if ( rc != 0 )
            ErrMsg( "concatenator.c execute_concat_un_compressed() KDirectoryFileSize( '%s' ) -> %R", file1, rc );
        else
        {
            if ( !force && file_exists( dir, "%s", output_filename ) )
            {
                rc = RC( rcExe, rcFile, rcPacking, rcName, rcExists );
                ErrMsg( "concatenator.c execute_concat_un_compressed() creating ouput-file '%s' -> %R",
                        output_filename, rc );
            }
            else
            {
                /* first try to create the output-file, so that sub-directories that do not exist
                   are created ... */
                struct KFile * dst;
                uint32_t files_offset = 1;
                    
                /* try to move the first file into the place of the output-file */
                rc = KDirectoryRename ( dir, true, file1, output_filename );
                if ( rc != 0 )
                {
                    /* this can fail, if file1 and output_filename are on different filesystems ... */
                    files_offset = 0;   /* this will make sure that we copy all files ( including the 1st one ) */
                    size_file1 = 0;
                    rc = KDirectoryCreateFile( dir, &dst, false, 0664, kcmInit, "%s", output_filename );
                    if ( rc != 0 )
                        ErrMsg( "concatenator.c execute_concat_un_compressed() KDirectoryCreateFile( '%s' ) -> %R",
                                output_filename, rc );
                }
                else
                {
                    rc = KDirectoryOpenFileWrite ( dir, &dst, true, "%s", output_filename );
                    if ( rc != 0 )
                        ErrMsg( "concatenator.c execute_concat_un_compressed() KDirectoryOpenFileWrite( '%s' ) -> %R",
                                output_filename, rc );
                }

                if ( rc == 0 )
                {
                    if ( buf_size > 0 )
                    {
                        struct KFile * tmp;
                        rc = KBufFileMakeWrite( &tmp, dst, false, buf_size );
                        if ( rc != 0 )
                            ErrMsg( "concatenator.c execute_concat_un_compressed() KBufFileMakeWrite( '%s' ) -> %R",
                                    output_filename, rc );
                        else
                        {
                            KFileRelease( dst );
                            dst = tmp;
                        }
                    }

                    bg_progress_update( progress, size_file1 );

                    rc = make_a_copy( dir, dst, files, progress, size_file1, buf_size, 
                                      files_offset, q_wait_time ); /* copy_machine.c */

                    KFileRelease( dst );
                }
            }
        }
    }
    return rc;
}

rc_t execute_concat_un_compressed( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files,
                    size_t buf_size,
                    struct bg_progress * progress,
                    bool force,
                    bool append,
                    uint32_t count,
                    uint32_t q_wait_time )
{
    rc_t rc = 0;
    bool perform_append = ( append && file_exists( dir, "%s", output_filename ) );
    if ( perform_append )
    {
        rc = execute_concat_un_compressed_append( dir, output_filename, files,
                            buf_size, progress, count, q_wait_time );
    }
    else
    {
        rc = execute_concat_un_compressed_no_append( dir, output_filename, files,
                            buf_size, progress, force, count, q_wait_time );
    }
    return rc;
}
                    
                    
/* ---------------------------------------------------------------------------------- */
                    
rc_t execute_concat( KDirectory * dir,
                    const char * output_filename,
                    const struct VNamelist * files,
                    size_t buf_size,
                    struct bg_progress * progress,
                    bool force,
                    bool append,
                    compress_t compress )
{
    uint32_t count;
    rc_t rc = VNameListCount( files, &count );
    if ( rc != 0 )
        ErrMsg( "concatenator.c execute_concat().VNameListCount() -> %R", rc );
    else if ( count > 0 )
    {
        uint32_t q_wait_time = 500;
        if ( compress != ct_none )
        {
            rc = execute_concat_compressed( dir, output_filename, files, buf_size,
                        progress, force, append, compress, count, q_wait_time ); /* above */
        }
        else
        {
            rc = execute_concat_un_compressed( dir, output_filename, files, buf_size,
                        progress, force, append, count, q_wait_time ); /* avove */
        }
    }
    return rc;
}
