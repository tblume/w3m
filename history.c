/* $Id: history.c,v 1.11 2003/09/26 17:59:51 ukai Exp $ */
#include "fm.h"
#include <errno.h>

#ifdef USE_HISTORY
Buffer *
historyBuffer(Hist *hist)
{
    Str src = Strnew();
    HistItem *item;
    char *p, *q;

    /* FIXME: gettextize? */
    Strcat_charp(src, "<html>\n<head><title>History Page</title></head>\n");
    Strcat_charp(src, "<body>\n<h1>History Page</h1>\n<hr>\n");
    Strcat_charp(src, "<ol>\n");
    if (hist && hist->list) {
	for (item = hist->list->last; item; item = item->prev) {
	    q = html_quote((char *)item->ptr);
	    if (DecodeURL)
		p = html_quote(url_decode2((char *)item->ptr, NULL));
	    else
		p = q;
	    Strcat_charp(src, "<li><a href=\"");
	    Strcat_charp(src, q);
	    Strcat_charp(src, "\">");
	    Strcat_charp(src, p);
	    Strcat_charp(src, "</a>\n");
	}
    }
    Strcat_charp(src, "</ol>\n</body>\n</html>");
    return loadHTMLString(src);
}

void
loadHistory(Hist *hist)
{
    FILE *f;
    Str line;
#define FNAMELEN 255
    char fname[FNAMELEN+1] = HISTORY_FILE;


    if (hist == NULL)
	return;
    if (Session) {
        strncat(fname, ".", FNAMELEN -6 - strlen(fname));
        strncat(fname, Session, FNAMELEN -6 - strlen(fname));
    }
    if ((f = fopen(rcFile(fname), "rt")) == NULL) {
	if (errno != ENOENT)
	    perror("error reading history file");
	return;
    }

    while (!feof(f)) {
	line = Strfgets(f);
	Strchop(line);
	Strremovefirstspaces(line);
	Strremovetrailingspaces(line);
	if (line->length == 0)
	    continue;
	pushHist(hist, url_quote(line->ptr));
    }
    fclose(f);
}

void
saveHistory(Hist *hist, size_t size)
{
    FILE *f, *h = NULL;
    HistItem *item;
    char *tmpf;
    int rename_ret;
#define FNAMELEN 255
    char fname[FNAMELEN+1] = HISTORY_FILE;
    char buf[4096];
    size_t rs, ws, remaining;

    if (hist == NULL || hist->list == NULL)
	return;
    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    if ((f = fopen(tmpf, "w")) == NULL) {
	/* FIXME: gettextize? */
	disp_err_message("Can't open history", FALSE);
	return;
    }
    for (item = hist->list->first; item && hist->list->nitem > size;
	 item = item->next)
	size++;
    for (; item; item = item->next)
	fprintf(f, "%s\n", (char *)item->ptr);
    if (fclose(f) == EOF) {
	/* FIXME: gettextize? */
	disp_err_message("Can't save history", FALSE);
	return;
    }

    if (Session) {
       strncat(fname, ".", FNAMELEN -6 - strlen(fname));
       strncat(fname, Session, FNAMELEN -6 - strlen(fname));
    }
    rename_ret = rename(tmpf, rcFile(fname));

    if (rename_ret == -1 && errno == EXDEV) {
       if ((f = fopen(tmpf, "r")) && (h = fopen(rcFile(fname), "w"))) {
           while (1) {
               rs = fread(buf, 1, sizeof(buf), f);
               if (rs == 0 || rs > sizeof(buf))
                       break;
               ws = fwrite(buf, 1, rs, h);
               if (ws == rs)
                       continue;
               if (ws == 0 || ws > rs)
                       break;
               remaining = rs - ws;
               while (remaining > 0) {
                       ws = fwrite(buf + (rs - remaining), 1, remaining, h);
                       if (ws == 0 || ws > remaining)
                               break;
                       remaining -= ws;
               }
           }
       }
       if (f) fclose(f);
       if (h) fclose(h);
    } else if (rename_ret != 0) {
	disp_err_message("Can't save history", FALSE);
	return;
    }
}
#endif				/* USE_HISTORY */

Hist *
newHist()
{
    Hist *hist;

    hist = New(Hist);
    hist->list = (HistList *)newGeneralList();
    hist->current = NULL;
    hist->hash = NULL;
    return hist;
}

Hist *
copyHist(Hist *hist)
{
    Hist *new;
    HistItem *item;

    if (hist == NULL)
	return NULL;
    new = newHist();
    for (item = hist->list->first; item; item = item->next)
	pushHist(new, (char *)item->ptr);
    return new;
}

HistItem *
unshiftHist(Hist *hist, char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    item = (HistItem *)newListItem((void *)allocStr(ptr, -1),
				   (ListItem *)hist->list->first, NULL);
    if (hist->list->first)
	hist->list->first->prev = item;
    else
	hist->list->last = item;
    hist->list->first = item;
    hist->list->nitem++;
    return item;
}

HistItem *
pushHist(Hist *hist, char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    item = (HistItem *)newListItem((void *)allocStr(ptr, -1),
				   NULL, (ListItem *)hist->list->last);
    if (hist->list->last)
	hist->list->last->next = item;
    else
	hist->list->first = item;
    hist->list->last = item;
    hist->list->nitem++;
    return item;
}

/* Don't mix pushHashHist() and pushHist()/unshiftHist(). */

HistItem *
pushHashHist(Hist *hist, char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    item = getHashHist(hist, ptr);
    if (item) {
	if (item->next)
	    item->next->prev = item->prev;
	else			/* item == hist->list->last */
	    hist->list->last = item->prev;
	if (item->prev)
	    item->prev->next = item->next;
	else			/* item == hist->list->first */
	    hist->list->first = item->next;
	hist->list->nitem--;
    }
    item = pushHist(hist, ptr);
    putHash_sv(hist->hash, ptr, (void *)item);
    return item;
}

HistItem *
getHashHist(Hist *hist, char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->hash == NULL) {
	hist->hash = newHash_sv(HIST_HASH_SIZE);
	for (item = hist->list->first; item; item = item->next)
	    putHash_sv(hist->hash, (char *)item->ptr, (void *)item);
    }
    return (HistItem *)getHash_sv(hist->hash, ptr, NULL);
}

char *
lastHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->list->last) {
	hist->current = hist->list->last;
	return (char *)hist->current->ptr;
    }
    return NULL;
}

char *
nextHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->current && hist->current->next) {
	hist->current = hist->current->next;
	return (char *)hist->current->ptr;
    }
    return NULL;
}

char *
prevHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->current && hist->current->prev) {
	hist->current = hist->current->prev;
	return (char *)hist->current->ptr;
    }
    return NULL;
}
