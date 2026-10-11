

#ifndef __NOLPHIN_RECENT_H__
#define __NOLPHIN_RECENT_H__

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-file.h>
#include <gio/gio.h>

void nolphin_recent_add_file (NolphinFile *file,
			       GAppInfo *application);

#endif
