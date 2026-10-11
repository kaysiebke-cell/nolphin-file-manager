/*
 *  nolphin-name-and-desc-provider.h - Interface for Nolphin extensions that 
 *  returns the extension's proper name and description for the plugin
 *  manager only - it is not necessary for extension functionality.
 *
 */

#ifndef NOLPHIN_NAME_AND_DESC_PROVIDER_H
#define NOLPHIN_NAME_AND_DESC_PROVIDER_H

#include <glib-object.h>
#include "nolphin-extension-types.h"

G_BEGIN_DECLS

#define NOLPHIN_TYPE_NAME_AND_DESC_PROVIDER (nolphin_name_and_desc_provider_get_type ())

G_DECLARE_INTERFACE (NolphinNameAndDescProvider, nolphin_name_and_desc_provider,
                     NOLPHIN, NAME_AND_DESC_PROVIDER,
                     GObject)

typedef NolphinNameAndDescProviderInterface NolphinNameAndDescProviderIface;

struct _NolphinNameAndDescProviderInterface {
    GTypeInterface g_iface;

    GList *(*get_name_and_desc) (NolphinNameAndDescProvider *provider);
};

/* Interface Functions */
GList *nolphin_name_and_desc_provider_get_name_and_desc (NolphinNameAndDescProvider *provider);

G_END_DECLS

#endif
