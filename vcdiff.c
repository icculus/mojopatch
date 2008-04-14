/**
 * MojoPatch; a tool for updating data in the field.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

/*
 * (How is there no vcdiff implementation under the zlib license?!)
 * Written based on the RFC: http://www.faqs.org/rfcs/rfc3284.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#if (defined(_MSC_VER) && !defined(inline))
#define inline _inline
#endif


static inline uint32 swapui32(uint32 x)
{
#if PLATFORM_BIGENDIAN
    return (((x)>>24) + (((x)>>8)&0xff00) + (((x)<<8)&0xff0000) + ((x)<<24));
#else
    return (x);
#endif
}

static void *internal_malloc(int bytes, void *d) { return malloc(bytes); }
static void internal_free(void *ptr, void *d) { free(ptr); }


static int64 stdio_read(void *ctx, void *buf, uint32 n)
{
    FILE *io = (FILE *) ctx;
    const int64 rc = (int64) fread(buf, 1, (size_t) n, io);
    if ( (rc == 0) && (ferror(io)) )
        return -1;
    return rc;
} /* stdio_read */

static int64 stdio_write(void *ctx, void *buf, uint32 n)
{
    FILE *io = (FILE *) ctx;
    const int64 rc = (int64) fwrite(buf, 1, (size_t) n, io);
    if ( (rc == 0) && (ferror(io)) )
        return -1;
    return rc;
} /* stdio_write */

static int64 stdio_seek(void *ctx, uint64 n)
{
    FILE *io = (FILE *) ctx;
    if (fseeko(io, (off_t) n, SEEK_SET) == -1)
        return -1;
    return (int64) ftello(io);
} /* stdio_seek */


/* More compact when you need: "this operation must not 'sort of' work". */
static inline int Read(vcdiff_io *io, void *buf, uint32 n)
{
    return ( io->read(io->ctx, buf, n) == ((int64) n) );
} /* Read */

/* More compact when you need: "this operation must not 'sort of' work". */
static inline int Write(vcdiff_io *io, void *buf, uint32 n)
{
    return ( io->write(io->ctx, buf, n) == ((int64) n) );
} /* Write */

/* More compact when you need: "this operation must not 'sort of' work". */
static inline int Seek(vcdiff_io *io, uint64 pos)
{
    return ( io->seek(io->ctx, pos) == ((int64) pos) );
} /* Seek */


static int Read_ui32(vcdiff_io *io, uint32 *ui32)
{
    if (!Read(io, ui32, sizeof (*ui32)))
        return 0;
    *ui32 = swapui32(*ui32);
    return 1;
} /* Read_ui32 */


typedef struct
{
    vcdiff_io *src;
    vcdiff_io *delta;
    vcdiff_io *dst;
    uint8 compressor;
    uint32 tablelen;
    uint8 *codetable;
    vcdiff_malloc malloc;
    vcdiff_free free;
    void *malloc_data;
} vcdiff_ctx;


/* Convenience functions for allocators... */

static inline void *Malloc(const vcdiff_ctx *ctx, const int len)
{
    return ctx->malloc(len, ctx->malloc_data);
} /* Malloc */


static inline void Free(const Context *ctx, void *ptr)
{
    if (ptr != NULL) /* check for NULL in case of dumb free() impl. */
        ctx->free(ptr, ctx->malloc_data);
} /* Free */


static int read_delta_header(vcdiff_ctx *ctx)
{
    vcdiff_io *io = ctx->delta;
    uint8 sig[5];
    if (!Read(io, sig, sizeof (sig)))
        return 0;

    /* magic signature. */
    if ((sig[0]!=0xD6) || (sig[1]!=0xC3) || (sig[2]!=0xC4) || (sig[3]!=0x00))
        return 0;   /* not a delta file. */
    else
    {
        const uint8 indicator = sig[4];
        const int has_compressor = (indicator & (1 << 0)) ? 1 : 0;
        const int has_codetable = (indicator & (1 << 1)) ? 1 : 0;

        if ((indicator & 0xFC) != 0)
            return 0;  /* bits we weren't expecting are set. */

        if (has_compressor)
        {
            if (!Read(io, &ctx->compressor, sizeof (compressor)))
                return 0;
            return 0;  /* !!! FIXME: unsupported at the moment. */
        } /* if */

        if (has_codetable)
        {
            if (!Read_ui32(io, &ctx->tablelen))
                return 0;
            ctx->codetable = (uint8 *) Malloc(ctx, (size_t) ctx->tablelen);
            if (ctx->codetable == NULL)
                return 0;
            if (!Read(io, ctx->codetable, ctx->tablelen))
                return 0;
            return 0;  /* !!! FIXME: unsupported at the moment. */
        } /* if */
    } /* else */
    
    return 1;
} /* read_delta_header */

static int _read_delta_window(vcdiff_ctx *ctx, const uint8 indicator)
{
    vcdiff_io *io = ctx->delta;
    const int source = (indicator & (1 << 0)) ? 1 : 0;
    const int target = (indicator & (1 << 1)) ? 1 : 0;
    uint32 len = 0;
    uint32 pos = 0;
    vcdiff_io *srcio = NULL;
    uint8 *srcdata = NULL;

    if ((indicator & 0xFC) != 0)
        return 0;  /* bits we weren't expecting are set. */
    else if ((source) && (target))
        return 0;  /* can't have both! */
    else if ((source) || (target))
    {
        if (!Read_ui32(io, &len))
            return 0;
        else if (!Read_ui32(io, &pos))
            return 0;
        srcio = (source) ? ctx->src : ctx->dst;
        srcdata = (uint8 *) Malloc(ctx, len);
        if (srcdata == NULL)
            return 0;
        if ( (!Seek(srcio, pos)) || (!Read(srcio, srcdata, len)) )
        {
            Free(src, srcdata);
            return 0;
        } /* if */
    } /* else if */

    

    Free(ctx, srcdata);
} /* _read_delta_window */

static int read_delta_window(vcdiff_ctx *ctx)
{
    vcdiff_io *io = ctx->delta;
    uint8 indicator;
    int64 br = io->read(io->ctx, &indicator, sizeof (indicator));
    if (br == 0)
        return 0;  /* EOF. We're done! */
    else if (br == -1)
        return -1; /* Error. We're also done. */
    return _read_delta_window(ctx, indicator);
} /* read_delta_window */


/* so all logic uses ctx, instead of the mainline using &ctx ... */
static int _vcdiff(vcdiff_ctx *ctx)
{
    if (!read_delta_header(ctx))
        return 0;
    else
    {
        int rc;
        while ((rc = read_delta_window(ctx)) == 1) { /* keep looping. */ }
        if (rc == -1)
            return 0;  /* error, not successful EOF. */
    } /* else */

    return 1;  /* success. */
} /* _vcdiff */


int vcdiff(vcdiff_io *iosrc, vcdiff_io *iodelta, vcdiff_io *iodst,
           vcdiff_malloc m, vcdiff_free f, void *d)
{
    int retval = 0;
    vcdiff_ctx ctx;
    memset(&ctx, '\0', sizeof (ctx));
    ctx.iosrc = iosrc;
    ctx.iodelta = iodelta;
    ctx.iodst = iodst;
    ctx.malloc = (m != NULL) ? m : internal_malloc;
    ctx.free = (f != NULL) ? f : internal_free;
    ctx.malloc_data = d;
    retval = _vcdiff(&ctx);
    Free(&ctx, ctx.codetable);
    return retval;
} /* vcdiff */


/* Please make sure all are seekable! */
int vcdiff_stdio(FILE *fiosrc, FILE *fiodelta, FILE *fiodst,
                 vcdiff_malloc m, vcdiff_free f, void *d)

{
    vcdiff_io iosrc = { stdio_read, stdio_write, stdio_seek, fiosrc };
    vcdiff_io iodelta = { stdio_read, stdio_write, stdio_seek, fiodelta };
    vcdiff_io iodst = { stdio_read, stdio_write, stdio_seek, fiodst };
    return vcdiff(&iosrc, &iodelta, &iodst, m, f, d);
} /* vcdiff_stdio */


/* All three of these are filenames. */
int vcdiff_fname(const char *src, const char *delta, const char *dst,
                 vcdiff_malloc m, vcdiff_free f, void *d)
{
    FILE *iosrc = fopen(src, "rb");
    FILE *iodelta = fopen(delta, "rb");
    FILE *iodst = fopen(dst, "r+b");
    const int rc = vcdiff_stdio(iosrc, iodelta, iodst, m, f, d);

    fclose(iosrc);
    fclose(iodelta);
    fclose(iodst);

    if (!rc)
        unlink(dst);

    return rc;
} /* vcdiff_fname */

/* end of vcdiff.c ... */

