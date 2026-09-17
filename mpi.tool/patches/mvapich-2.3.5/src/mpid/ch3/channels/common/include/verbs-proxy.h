/* Copyright (c) 2001-2019, The Ohio State University. All rights
 * reserved.
 *
 * This file is part of the MVAPICH software package developed by the
 * team members of The Ohio State University's Network-Based Computing
 * Laboratory (NBCL), headed by Professor Dhabaleswar K. (DK) Panda.
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level MVAPICH2 directory.
 *
 */

#ifndef VERBS_PROXY_H
#define VERBS_PROXY_H

#ifndef INFINIBAND_VERBS_H
#error "Proxy verbs should be included after verbs."
#endif


#define ibv_ack_cq_events        mv2_proxy_ibv_ack_cq_events
void ibv_ack_cq_events(struct ibv_cq *cq, unsigned int nevents);

#define ibv_ack_async_event      mv2_proxy_ibv_ack_async_event
void ibv_ack_async_event(struct ibv_async_event *event);

#define ibv_alloc_pd             mv2_proxy_ibv_alloc_pd
struct ibv_pd *ibv_alloc_pd(struct ibv_context *context);

#define ibv_attach_mcast         mv2_proxy_ibv_attach_mcast
int ibv_attach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid);

#define ibv_close_device         mv2_proxy_ibv_close_device
int ibv_close_device(struct ibv_context *context);

#define ibv_create_ah            mv2_proxy_ibv_create_ah
struct ibv_ah *ibv_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr);

#define ibv_create_comp_channel  mv2_proxy_ibv_create_comp_channel
struct ibv_comp_channel *ibv_create_comp_channel(struct ibv_context *context);

#define ibv_create_cq            mv2_proxy_ibv_create_cq
struct ibv_cq *ibv_create_cq(struct ibv_context *context, int cqe,void *cq_context,struct ibv_comp_channel *channel,int comp_vector);

#define ibv_create_srq           mv2_proxy_ibv_create_srq
struct ibv_srq *ibv_create_srq(struct ibv_pd *pd,struct ibv_srq_init_attr *srq_init_attr);

#define ibv_create_qp            mv2_proxy_ibv_create_qp
struct ibv_qp *ibv_create_qp(struct ibv_pd *pd,struct ibv_qp_init_attr *qp_init_attr);

#define ibv_dealloc_pd           mv2_proxy_ibv_dealloc_pd
int ibv_dealloc_pd(struct ibv_pd *pd);

#define ibv_dereg_mr             mv2_proxy_ibv_dereg_mr
int ibv_dereg_mr(struct ibv_mr *mr);

#define ibv_destroy_ah           mv2_proxy_ibv_destroy_ah
int ibv_destroy_ah(struct ibv_ah *ah);

#define ibv_destroy_comp_channel mv2_proxy_ibv_destroy_comp_channel
int ibv_destroy_comp_channel(struct ibv_comp_channel *channel);

#define ibv_destroy_cq           mv2_proxy_ibv_destroy_cq
int ibv_destroy_cq(struct ibv_cq *cq);

#define ibv_destroy_qp           mv2_proxy_ibv_destroy_qp
int ibv_destroy_qp(struct ibv_qp *qp);

#define ibv_detach_mcast         mv2_proxy_ibv_detach_mcast
int ibv_detach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid);

#define ibv_event_type_str       mv2_proxy_ibv_event_type_str
const char *ibv_event_type_str(enum ibv_event_type event);

#define ibv_free_device_list     mv2_proxy_ibv_free_device_list
void ibv_free_device_list(struct ibv_device **list);

#define ibv_get_async_event      mv2_proxy_ibv_get_async_event
int ibv_get_async_event(struct ibv_context *context,struct ibv_async_event *event);

#define ibv_get_device_list      mv2_proxy_ibv_get_device_list
struct ibv_device **ibv_get_device_list(int *num_devices);

#define ibv_get_device_name      mv2_proxy_ibv_get_device_name
const char *ibv_get_device_name(struct ibv_device *device);

#define ibv_get_cq_event         mv2_proxy_ibv_get_cq_event
int ibv_get_cq_event(struct ibv_comp_channel *channel,struct ibv_cq **cq, void **cq_context);

#define ibv_modify_qp            mv2_proxy_ibv_modify_qp
int ibv_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,int attr_mask);

#define ibv_modify_srq           mv2_proxy_ibv_modify_srq
int ibv_modify_srq(struct ibv_srq *srq,struct ibv_srq_attr *srq_attr,int srq_attr_mask);

#define ibv_open_device          mv2_proxy_ibv_open_device
struct ibv_context *ibv_open_device(struct ibv_device *device);

#define ibv_poll_cq              mv2_proxy_ibv_poll_cq
int ibv_poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc); /* originaly inlined */

#define ibv_post_send            mv2_proxy_ibv_post_send
int ibv_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr); /* originaly inlined */

#define ibv_post_recv            mv2_proxy_ibv_post_recv
int ibv_post_recv(struct ibv_qp *qp, struct ibv_recv_wr *wr, struct ibv_recv_wr **bad_wr); /* originaly inlined */

#define ibv_post_srq_recv        mv2_proxy_ibv_post_srq_recv
int ibv_post_srq_recv(struct ibv_srq *srq, struct ibv_recv_wr *recv_wr, struct ibv_recv_wr **bad_recv_wr); /* originaly inlined */

#define ibv_query_device         mv2_proxy_ibv_query_device
int ibv_query_device(struct ibv_context *context, struct ibv_device_attr *device_attr);

#define ibv_query_gid            mv2_proxy_ibv_query_gid
int ibv_query_gid(struct ibv_context *context, uint8_t port_num, int index, union ibv_gid *gid);

#define ibv_query_pkey           mv2_proxy_ibv_query_pkey
int ibv_query_pkey(struct ibv_context *context, uint8_t port_num, int index, uint16_t *pkey);

#undef ibv_query_port
#define ibv_query_port           mv2_proxy_ibv_query_port
int ibv_query_port(struct ibv_context *context, uint8_t port_num, struct ibv_port_attr *port_attr);

#define ibv_query_qp             mv2_proxy_ibv_query_qp
int ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr);

#undef ibv_reg_mr               
#define ibv_reg_mr               mv2_proxy_ibv_reg_mr
struct ibv_mr *ibv_reg_mr(struct ibv_pd *pd, void *addr, size_t length, int access);

#define ibv_req_notify_cq        mv2_proxy_ibv_req_notify_cq
int ibv_req_notify_cq(struct ibv_cq *cq, int solicited_only); /* originaly inlined */



#endif /* VERBS_PROXY_H */
