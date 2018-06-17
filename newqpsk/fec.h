#ifndef _FEC_H
#define _FEC_H

/* --------------------------------------------------------------------- */

struct fecstate {
	int feclevel;
	int bitbatchlen;
	int inlv;
	unsigned inlvpipe[MaxInlv * DataCarriers];
	unsigned inlvptr;
};

/* --------------------------------------------------------------------- */

extern inline void init_fec(struct fecstate *f)
{
	switch (f->feclevel) {
	case 0:
		f->bitbatchlen = 15;
		break;
	case 1:
		f->bitbatchlen = 11;
		break;
	case 2:
		f->bitbatchlen = 7;
		break;
	case 3:
		f->bitbatchlen = 5;
		break;
	}
}

/* --------------------------------------------------------------------- */

extern inline void init_inlv(struct fecstate *f)
{
        int i;

        for (i = 0; i < f->inlv * DataCarriers; i++)
                f->inlvpipe[i] = 0;
	f->inlvptr = 0;
}

/* --------------------------------------------------------------------- */

extern unsigned deinlv(struct fecstate *, unsigned);
extern unsigned inlv(struct fecstate *, unsigned);

extern unsigned fecencode(struct fecstate *, unsigned);
extern unsigned fecdecode(struct fecstate *, unsigned, unsigned *);

/* --------------------------------------------------------------------- */

#endif
