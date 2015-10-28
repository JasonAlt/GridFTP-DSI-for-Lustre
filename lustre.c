/*
 * University of Illinois/NCSA Open Source License
 *
 * Copyright © 2015 NCSA.  All rights reserved.
 *
 * Developed by:
 *
 * Storage Enabling Technologies (SET)
 *
 * Nation Center for Supercomputing Applications (NCSA)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the .Software.),
 * to deal with the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 *    + Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimers.
 *
 *    + Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimers in the
 *      documentation and/or other materials provided with the distribution.
 *
 *    + Neither the names of SET, NCSA
 *      nor the names of its contributors may be used to endorse or promote
 *      products derived from this Software without specific prior written
 *      permission.
 *
 * THE SOFTWARE IS PROVIDED .AS IS., WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 */

/*
 * System includes. 
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

/*
 *  Lustre includes.
 */
#include <lustre/liblustreapi.h>
#include <lustre/lustre_user.h>

/*
 * Globus includes
 */
#include <globus_gridftp_server.h>

#define TERABYTE ((globus_off_t)1024 * 1024 * 1024 * 1024)
#define GIGABYTE ((globus_off_t)1024 * 1024 * 1024)
#define MEGABYTE ((globus_off_t)1024 * 1024)
#define DEFAULT_STRIPE_SIZE (MEGABYTE)
#define LUSTRE_SUPER_MAGIC 0xbd00bd0

/*
 * Could use __attribute__((constructor)) void init(void) { ... }
 */


/*
 * Returns:
 *    0 on success and is_lustre is 1 for true, 0 for false
 *    -1 on error and errno is set
 */
globus_result_t
is_lustre_file(const char * pathname, int * is_lustre)
{
    int             retval      = 0;
    char          * path_copy   = NULL;
    char          * parent_path = NULL;
	globus_result_t result      = GLOBUS_SUCCESS;
    struct statfs   statbuf;

	GlobusGFSName(__func__);

    /* Copy the pathname. */
    path_copy = strdup(pathname);
    if (path_copy == NULL)
	{
		result = GlobusGFSErrorSystemError("strdup", ENOMEM);
		goto cleanup;
	}

    /* Get the parent directory name. */
    parent_path = dirname(path_copy);

    /* Stat the parent directory */
    retval = statfs(parent_path, &statbuf);
    if (retval != 0)
    {
		result = GlobusGFSErrorSystemError("statfs", errno);
		goto cleanup;
    }

    switch (statbuf.f_type)
    {
    case LUSTRE_SUPER_MAGIC:
#ifdef DEBUG
        printf ("type: Lustre\n");
#endif /* DEBUG */
        *is_lustre = 1;
        break;
    default:
        *is_lustre = 0;
#ifdef DEBUG
        printf ("type: %x\n", statbuf.f_type);
#endif /* DEBUG */
        break;
    }

cleanup:
    if (path_copy)
        free(path_copy);

    return result;
}

static int
convert_allo_to_stripe_count(globus_off_t alloc_size)
{
	/* Calculate stripe count based upon the TB count. */
	return (alloc_size / (2*TERABYTE)) + ((alloc_size % (2*TERABYTE)) != 0);
}

static inline int maxint(int a, int b)
{
	return a > b ? a : b;
}

static void *
alloc_lum()
{
	int v1, v3;

	v1 = sizeof(struct lov_user_md_v1) +
	            LOV_MAX_STRIPE_COUNT * sizeof(struct lov_user_ost_data_v1);
	v3 = sizeof(struct lov_user_md_v3) +
	            LOV_MAX_STRIPE_COUNT * sizeof(struct lov_user_ost_data_v1);

	return malloc(maxint(v1, v3));
}

static globus_result_t
get_parent_dir_stripe_count(const char * Pathname,
                            short      * StripeCount,
                            int        * StripeSize,
                            int        * StripePattern,
                            short      * StripeOffset)
{
	int    retval   = 0;
	char * tmp_str  = strdup(Pathname);
	char * dir_name = dirname(tmp_str);
	struct lov_user_md * lum    = alloc_lum();
	globus_result_t      result = GLOBUS_SUCCESS;

	GlobusGFSName(get_parent_dir_stripe_count);

	retval = llapi_file_get_stripe(dir_name, lum);
	switch (retval)
	{
	case 0:
		*StripeCount   = lum->lmm_stripe_count;
		*StripeSize    = lum->lmm_stripe_size;
		*StripeOffset  = lum->lmm_stripe_offset;
		*StripePattern = lum->lmm_pattern;
		break;

	case -ENODATA:
		*StripeCount   = 1;
		*StripeSize    = DEFAULT_STRIPE_SIZE;
		*StripePattern = 0;
		*StripeOffset  = -1;
		break;

	default:
		result = GlobusGFSErrorSystemError("llapi_file_get_stripe", errno);
		break;
	}

	free(tmp_str);
	free(lum);

	return result;
}

globus_result_t
create_striped_file(const char * pathname, globus_off_t file_size)
{
	int   retval                = 0;
	int   stripe_count          = 0;
	short parent_stripe_count   = 0;
	short parent_stripe_offset  = 0;
	int   parent_stripe_size    = 0;
	int   parent_stripe_pattern = 0;
	globus_result_t result  = GLOBUS_SUCCESS;

	GlobusGFSName(create_striped_file);

	/* Determine the stripe count based on the file size. */
	stripe_count = convert_allo_to_stripe_count(file_size);

	/* See if the parent directory has other settings. */
	result = get_parent_dir_stripe_count(pathname, 
	                                     &parent_stripe_count, 
	                                     &parent_stripe_size, 
	                                     &parent_stripe_pattern, 
	                                     &parent_stripe_offset);
	if (result)
		goto cleanup;

	if (stripe_count > parent_stripe_count)
	{
		/* Now create the file. */
		retval = llapi_file_create(pathname,
		                           parent_stripe_size,     /* Stripe Size - multiple of 64KiB */
		                           parent_stripe_offset,   /* Stripe Offset */
		                           stripe_count,           /* Stripe Count */
		                           parent_stripe_pattern); /* Stripe Pattern */

		if (retval < 0 && retval != -EEXIST && retval != -EALREADY && retval != -ENOTTY)
			result = GlobusGFSErrorSystemError("llapi_file_create", -retval);
	}

cleanup:
	return result;
}
