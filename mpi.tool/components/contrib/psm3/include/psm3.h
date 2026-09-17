#ifndef PSM3_H
#define PSM3_H

/* psm2.h/psm2_mq.h supply the common opaque types (psm2_error_t, psm2_ep_t,
 * psm2_mq_t, etc.) shared by both classic OPA-PSM2 and the PSM3 provider. */
#include <psm2.h>
#include <psm2_mq.h>
#include <psm2_am.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The PSM3 provider (libpsm3-fi) renames the psm2_* entry points below to
 * psm3_*, to avoid symbol collisions with a classic libpsm2 loaded in the
 * same process. Installed psm2.h/psm2_mq.h headers may be the classic
 * OPA-PSM2 ones, which only declare the psm2_* names, so declare the
 * psm3_* prototypes explicitly here rather than relying on any installed
 * header to provide them.
 */

/* opaque, same layout as struct psm2_ep_open_opts */
struct psm3_ep_open_opts;

psm2_error_t psm3_init(int *api_verno_major, int *api_verno_minor);
psm2_error_t psm3_finalize(void);
psm2_error_t psm3_poll(psm2_ep_t ep);
psm2_error_t psm3_ep_open(const psm2_uuid_t unique_job_key,
			   const struct psm3_ep_open_opts *opts,
			   psm2_ep_t *ep, psm2_epid_t *epid);
psm2_error_t psm3_ep_close(psm2_ep_t ep, int mode, int64_t timeout);
psm2_error_t psm3_ep_connect(psm2_ep_t ep, int num_of_epid,
			      const psm2_epid_t *array_of_epid,
			      const int *array_of_epid_mask,
			      psm2_error_t *array_of_errors,
			      psm2_epaddr_t *array_of_epaddr,
			      int64_t timeout);

psm2_error_t psm3_mq_init(psm2_ep_t ep, uint64_t ignored,
			   const struct psm2_optkey *opts, int numopts,
			   psm2_mq_t *mq);
psm2_error_t psm3_mq_finalize(psm2_mq_t mq);
psm2_error_t psm3_mq_irecv2(psm2_mq_t mq, psm2_epaddr_t src,
			     psm2_mq_tag_t *rtag, psm2_mq_tag_t *rtagsel,
			     uint32_t flags, void *buf, uint32_t len,
			     void *context, psm2_mq_req_t *req);
psm2_error_t psm3_mq_imrecv(psm2_mq_t mq, uint32_t flags, void *buf,
			     uint32_t len, void *context,
			     psm2_mq_req_t *reqo);
psm2_error_t psm3_mq_send2(psm2_mq_t mq, psm2_epaddr_t dest, uint32_t flags,
			     psm2_mq_tag_t *stag, const void *buf,
			     uint32_t len);
psm2_error_t psm3_mq_isend2(psm2_mq_t mq, psm2_epaddr_t dest, uint32_t flags,
			      psm2_mq_tag_t *stag, const void *buf,
			      uint32_t len, void *context,
			      psm2_mq_req_t *req);
psm2_error_t psm3_mq_iprobe2(psm2_mq_t mq, psm2_epaddr_t src,
			       psm2_mq_tag_t *rtag, psm2_mq_tag_t *rtagsel,
			       psm2_mq_status2_t *status);
psm2_error_t psm3_mq_improbe2(psm2_mq_t mq, psm2_epaddr_t src,
				psm2_mq_tag_t *rtag, psm2_mq_tag_t *rtagsel,
				psm2_mq_req_t *reqo,
				psm2_mq_status2_t *status);
psm2_error_t psm3_mq_ipeek2(psm2_mq_t mq, psm2_mq_req_t *req,
			      psm2_mq_status2_t *status);
psm2_error_t psm3_mq_ipeek_dequeue(psm2_mq_t mq, psm2_mq_req_t *req);
psm2_error_t psm3_mq_req_free(psm2_mq_t mq, psm2_mq_req_t req);
psm2_error_t psm3_mq_wait2(psm2_mq_req_t *request, psm2_mq_status2_t *status);
psm2_error_t psm3_mq_test2(psm2_mq_req_t *request, psm2_mq_status2_t *status);
psm2_error_t psm3_mq_cancel(psm2_mq_req_t *req);

#ifdef __cplusplus
}
#endif

#endif /* PSM3_H */
