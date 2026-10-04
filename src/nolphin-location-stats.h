/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-location-stats.h: Verlauf und "Häufig verwendet"
 *
 * Nolphin zählt selbst, welche Orte besucht werden (§13), und merkt sich
 * die zuletzt besuchten Orte. Beides wird persistent in
 * ~/.local/share/nolphin/locations.ini gespeichert.
 */

#ifndef NOLPHIN_LOCATION_STATS_H
#define NOLPHIN_LOCATION_STATS_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_LOCATION_STATS (nolphin_location_stats_get_type ())
G_DECLARE_FINAL_TYPE (NolphinLocationStats, nolphin_location_stats, NOLPHIN, LOCATION_STATS, GObject)

/* Signal "changed": Verlauf oder Zähler haben sich geändert. */
NolphinLocationStats *nolphin_location_stats_get_default (void);

void   nolphin_location_stats_record       (NolphinLocationStats *stats, GFile *location);
/* Listen von URIs (g_free'd Strings, mit g_list_free_full (…, g_free) freigeben). */
GList *nolphin_location_stats_get_recent   (NolphinLocationStats *stats, guint max);
GList *nolphin_location_stats_get_frequent (NolphinLocationStats *stats, guint max);
void   nolphin_location_stats_clear        (NolphinLocationStats *stats);

G_END_DECLS

#endif /* NOLPHIN_LOCATION_STATS_H */
