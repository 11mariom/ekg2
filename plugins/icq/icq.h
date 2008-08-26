#ifndef __ICQ_ICQ_H
#define __ICQ_ICQ_H

#include <ekg/dynstuff.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>

typedef struct {
	int fd;
	int fd2;

	int flap_seq;		/* FLAP seq id */
	int snac_seq;		/* SNAC seq id */
	int snacmeta_seq;	/* META SNAC seq id */

	int ssi;		/* server-side-userlist? */
	int aim;		/* aim-ok? */
	int user_default_group; /* XXX ?wo? TEMP! We should support list of groups */
	string_t cookie;	/* connection login cookie */
	string_t stream_buf;
} icq_private_t;

int icq_send_pkt(session_t *s, string_t buf);

void icq_session_connected(session_t *s);
int icq_write_status(session_t *s);
void icq_handle_disconnect(session_t *s, const char *reason, int type);

#define icq_uid(target) protocol_uid("icq", target)

#define MIRANDAOK 1
#define MIRANDA_COMPILANT_CLIENT 1

#define ICQ_DEBUG_UNUSED_INFORMATIONS 1

#endif
