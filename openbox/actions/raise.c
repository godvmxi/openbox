#include "openbox/actions.h"
#include "openbox/stacking.h"
#include "openbox/window.h"
#include <syslog.h>
#include <stdio.h>
static gboolean run_func(ObActionsData *data, gpointer options);

void action_raise_startup(void)
{
    actions_register("Raise",
                     NULL, NULL,
                     run_func,
                     NULL, NULL);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    if (data->client) {
//	syslog(LOG_INFO,"raise before move");
        actions_client_move(data, TRUE);
	
//	syslog(LOG_INFO,"raise before raise");
        stacking_raise(CLIENT_AS_WINDOW(data->client));
//	syslog(LOG_INFO,"raise after raise");
        actions_client_move(data, FALSE);
    }

    return FALSE;
}
