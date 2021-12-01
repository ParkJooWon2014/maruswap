#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/cpumask.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/fcntl.h>

#include "ib.h"

static int serverport;
static char fip[INET_ADDRSTRLEN];
static char sip[INET_ADDRSTRLEN];
static char src_ip[INET_ADDRSTRLEN];
static char multicast_ip[INET_ADDRSTRLEN];

module_param_named(sport,serverport,int,0644);
module_param_string(fip,fip,INET_ADDRSTRLEN,0644);
module_param_string(sip,sip,INET_ADDRSTRLEN,0644);
module_param_string(myip,src_ip,INET_ADDRSTRLEN,0644);
module_param_string(mip,multicast_ip,INET_ADDRSTRLEN,0644);


int mbswap_rdma_read(struct page *page, u64 roffset);
int mbswap_multicast_write(struct page *page, u64 roffset);

int get_rrandom(void)
{
	ktime_t rdn = ktime_get_ns();
	return (rdn);
}

static int get_random(void)
{
	ktime_t rdn = ktime_get_ns();
	return (rdn % 256);
}

enum {
	NR_BINDERS = 256,
};

struct binder {
	atomic_t flags;
	volatile void *private;
};

struct binder binders[NR_BINDERS];

void init_binder(void){
	int i = 0 ;
	for (; i < NR_BINDERS; i++) {
		atomic_set(&binders[i].flags, 0);
	}
}

int __get_binder(void)
{
	int index;
	int old;
	struct timespec ;
	do {
		index = get_random();
		old = atomic_cmpxchg(&binders[index].flags, 0, 1);
	} while (old != 0);
	return (index);
}

void __put_binder(int index)
{
	if(atomic_read(&binders[index].flags) != 2){
		pr_err("Unable to put binder\n");
		return;
	}
	atomic_set(&binders[index].flags, 0);
}

static int ib_addone(struct ib_device *dev)
{
	pr_info("ib addone() = %s\n",dev->name);
	return 0;
}

static void ib_removeone(struct ib_device *dev, void* client_data)
{
	pr_info("ib removeone() = %s\n",dev->name);
}

static struct ib_client ib_client = {
	.name = "ib",
	.add =  ib_addone,
	.remove = ib_removeone,
};


inline static int get_req_for_page(struct ib_device *dev,
		u64 *dma, struct page *page)
{
	int ret = 0;

	*dma = ib_dma_map_page(dev,page,0,PAGE_SIZE,DMA_TO_DEVICE);
	if(unlikely(ib_dma_mapping_error(dev,*dma))){
		pr_err("Unable to dma paiing\n");
		ret = - ENOMEM;
		return ret;
	}

	ib_dma_sync_single_for_device(dev,*dma,PAGE_SIZE,DMA_TO_DEVICE);

	return ret;
}

inline static int get_req_for_buf(struct ib_device *dev,
		u64 *dma,void *buf, size_t size)
{
	int ret = 0;

	*dma = ib_dma_map_single(dev, buf, size, DMA_TO_DEVICE);
	if (unlikely(ib_dma_mapping_error(dev, *dma))) {
		pr_err("ib_dma_mapping_error\n");
		ret = -ENOMEM;
		return ret;
	}

	ib_dma_sync_single_for_device(dev, *dma, size, DMA_TO_DEVICE);

	return ret;
}

static void ib_rdma_qp_event(struct ib_event *e, void *c)
{
	pr_info("ib rdma_qp_event\n");
}

static int rdma_ctrl_create_qp(struct rdma_ctrl_t *rdma_ctrl)
{
	struct ib_qp_init_attr attrs;
	int ret = 0 ;

	memset(&attrs,0,sizeof(attrs));

	attrs.qp_type = IB_QPT_RC;
	attrs.send_cq = rdma_ctrl->snd_cq;
	attrs.recv_cq = rdma_ctrl->rcv_cq;

	attrs.event_handler = ib_rdma_qp_event;
	attrs.cap.max_send_wr = MARUSWAP_MAX_SEND_WR;
	attrs.cap.max_recv_wr = MARUSWAP_MAX_RECV_WR;
	attrs.cap.max_send_sge = 1;
	attrs.cap.max_recv_sge = 1;
	attrs.cap.max_inline_data = 64;

	ret = rdma_create_qp(rdma_ctrl->rdma,rdma_ctrl->pd,&attrs);
	if(ret){
		pr_err("Unable to create qp (%d)\n",ret);
	}else{
		pr_info("[%s] success  creating  qp!\n",id_to_msg(rdma_ctrl->id));
	}
	rdma_ctrl->qp = rdma_ctrl->rdma->qp;

	return ret;
}

static int multicast_ctrl_create_qp(struct multicast_ctrl_t *multicast_ctrl)
{
	struct ib_qp_init_attr attrs;
	int ret = 0 ;

	memset(&attrs,0,sizeof(attrs));

	attrs.qp_type = IB_QPT_UD;
	attrs.send_cq = multicast_ctrl->snd_cq;
	attrs.recv_cq = multicast_ctrl->rcv_cq;

	attrs.cap.max_send_wr = MARUSWAP_MAX_SEND_WR;
	attrs.cap.max_recv_wr = MARUSWAP_MAX_RECV_WR;
	attrs.cap.max_send_sge = 1;
	attrs.cap.max_recv_sge = 1;
	attrs.cap.max_inline_data = 64;

	ret = rdma_create_qp(multicast_ctrl->multicast,multicast_ctrl->pd,&attrs);

	if(ret){
		pr_err("Unable to create qp (%d)\n",ret);
	}else{
		multi_info("creating qp success \n");
	}

	multicast_ctrl->qp = multicast_ctrl->multicast->qp;
	return ret;
}

inline static int rdma_ctrl_wait_for_cm(struct rdma_ctrl_t *rdma_ctrl)
{
	wait_for_completion_interruptible_timeout(&rdma_ctrl->cm_done,
			msecs_to_jiffies(CONNECTION_TIMEOUT_MS) + 1);
	return rdma_ctrl->cm_error;
}

static int  rdma_addr_resolved(struct rdma_ctrl_t *rdma_ctrl)
{
	int ret = 0;

	pr_info("%s : START : %s\n",id_to_msg(rdma_ctrl->id),__FUNCTION__);

	rdma_ctrl->pd = ib_alloc_pd(rdma_ctrl->rdma->device,0);
	if(IS_ERR(rdma_ctrl->pd)){
		pr_err("Unable to alloc pd");
		goto out_alloc_pd;
	}

	rdma_ctrl->mr = ib_alloc_mr(rdma_ctrl->pd,IB_MR_TYPE_MEM_REG,64);
	if(IS_ERR(rdma_ctrl->mr)){
		multi_err("Unable to alloc mr");
		goto out_alloc_pd;
	}

	rdma_ctrl->snd_cq = ib_alloc_cq(rdma_ctrl->rdma->device,rdma_ctrl,4096,
			0,IB_POLL_DIRECT);
	if(IS_ERR(rdma_ctrl->snd_cq)){
		pr_err("Unable to alloc snd cq");
		goto out_alloc_cq;
	}

	rdma_ctrl->rcv_cq = ib_alloc_cq(rdma_ctrl->rdma->device,rdma_ctrl,4096,
			0,IB_POLL_DIRECT);
	if(IS_ERR(rdma_ctrl->rcv_cq)){
		pr_err("Unable to alloc rcv cq");
		goto out_alloc_cq;
	}

	ret = rdma_ctrl_create_qp(rdma_ctrl);
	if(unlikely(ret)){
		pr_err("Unable to create qp\n");
		goto out_alloc_qp;
	}

	ret = rdma_resolve_route(rdma_ctrl->rdma,CONNECTION_TIMEOUT_MS);
	if(unlikely(ret)){
		pr_err("Unable to route\n");
		goto out_alloc_qp;
	}

	return ret;


out_alloc_qp:

	ib_dealloc_pd(rdma_ctrl->pd);

out_alloc_cq:

	if(rdma_ctrl->rcv_cq)
		ib_free_cq(rdma_ctrl->rcv_cq);
	if(rdma_ctrl->snd_cq)
		ib_free_cq(rdma_ctrl->snd_cq);

out_alloc_pd:

	return -EINVAL;
}


static int rdma_route_resolved(struct rdma_ctrl_t *rdma_ctrl,
		struct rdma_conn_param *conn_params)
{
	int ret = 0 ;
	struct rdma_conn_param param;

	pr_info("[%s] : START : %s\n",id_to_msg(rdma_ctrl->id),__FUNCTION__);
	memset(&param,0x0,sizeof(param));

	param.qp_num = rdma_ctrl->qp->qp_num;
	param.flow_control = 1;
	param.responder_resources = 16;
	param.initiator_depth = 16;
	param.retry_count = 7;
	param.rnr_retry_count = 7;
	param.private_data = NULL;
	param.private_data_len = 0;

	ret = rdma_connect_locked(rdma_ctrl->rdma, &param);
	if (ret){
		pr_err("[%s] rdma_connect failed (%d)\n",id_to_msg(rdma_ctrl->id), ret);
		rdma_destroy_qp(rdma_ctrl->rdma);
		return -EINVAL;
	}
	return ret;
}

static int rdma_connect_established(struct rdma_ctrl_t * rdma_ctrl)
{
	int i = 0;
	struct ib_device *dev = rdma_ctrl->qp->device;
	u64 dma;
	int ret = 0 ;

	rdma_ctrl->rcv_buffer = kzalloc(PAGE_SIZE,GFP_KERNEL);
	if(!rdma_ctrl->rcv_buffer){
		pr_err("Unable to alloc rdma buffer\n");
		return 1;
	}
	ret = get_req_for_buf(dev,&dma,rdma_ctrl->rcv_buffer,PAGE_SIZE);
	if(ret){
		pr_err("Unable to get req for recv buf \n");
		return ret;
	}

	ret = get_req_for_buf(dev,&rdma_ctrl->dma_buffer,rdma_ctrl->buffer,PAGE_SIZE);
	if(ret){
		pr_err("Unable to get req for buf \n");
		return ret;
	}

	for(i=0; i < NR_RECV ; i ++){

		struct recv_work * rw = kzalloc(sizeof(*rw),GFP_KERNEL);
		const struct ib_recv_wr *bad_wr = NULL;
		uintptr_t b = dma + i * RPC_MAX_EACH_SIZE;

		*rw = (struct recv_work){
			.id = i,
				.size = RPC_MAX_EACH_SIZE,
				.buffer = rdma_ctrl->rcv_buffer + i*RPC_MAX_EACH_SIZE,
				.sg_list = {
					.addr = (uintptr_t)b,
					.length = RPC_MAX_EACH_SIZE,
					.lkey = rdma_ctrl->pd->local_dma_lkey,
				},
				.wr = {
					.next = NULL,
					.wr_id = (uintptr_t)rw,
					.sg_list = &rw->sg_list,
					.num_sge = 1,
				},
		};
		ret = ib_post_recv(rdma_ctrl->qp,&rw->wr,&bad_wr);
		if(ret){
			pr_err("[%s] Unable to post recv wr\n",id_to_msg(rdma_ctrl->id));
			return -EINVAL;
		}
		cpu_relax();
	}

	atomic_add(1,rdma_ctrl->nr_rdma_ctrl);
	rdma_ctrl->alive = true;

	pr_info("connection established\n");

	return 0;
}

static int ib_rdma_cm_handler(struct rdma_cm_id *cm_id,
		struct rdma_cm_event *ev)
{

	struct rdma_ctrl_t *rdma_ctrl = cm_id->context;
	int cm_error = 0;

	pr_info("[%s] cm_handler msg: %s (%d) status %d id %p\n",id_to_msg(rdma_ctrl->id), rdma_event_msg(ev->event),
			ev->event, ev->status, cm_id);

	switch (ev->event) {
		case RDMA_CM_EVENT_ADDR_RESOLVED:	
			cm_error = rdma_addr_resolved(rdma_ctrl);
			break;
		case RDMA_CM_EVENT_ROUTE_RESOLVED:
			cm_error = rdma_route_resolved(rdma_ctrl,&ev->param.conn);
			break;
		case RDMA_CM_EVENT_ESTABLISHED:
			cm_error = rdma_connect_established(rdma_ctrl);	
			complete(&rdma_ctrl->cm_done);
			break;
		case RDMA_CM_EVENT_REJECTED:
			pr_err("[%s] connection rejected\n",id_to_msg(rdma_ctrl->id));	
			cm_error = -EINVAL;
			break;
		case RDMA_CM_EVENT_ADDR_ERROR:
		case RDMA_CM_EVENT_ROUTE_ERROR:
		case RDMA_CM_EVENT_CONNECT_ERROR:
		case RDMA_CM_EVENT_UNREACHABLE:
			pr_err("CM error event %d\n", ev->event);
			cm_error = -ECONNRESET;
			break;
		case RDMA_CM_EVENT_DISCONNECTED:
		case RDMA_CM_EVENT_ADDR_CHANGE:
		case RDMA_CM_EVENT_TIMEWAIT_EXIT:
			pr_err("CM connection closed %d\n", ev->event);
			break;
		case RDMA_CM_EVENT_DEVICE_REMOVAL:
			break;
		default:
			pr_err("CM unexpected event: %d\n", ev->event);
			break;
	}

	if (cm_error) {
		rdma_ctrl->cm_error = cm_error;
		complete(&rdma_ctrl->cm_done);
	}
	if(rdma_ctrl->cm_error){
		return rdma_ctrl->cm_error;
	}
	return 0;
}

static int multicast_addr_resolved(struct multicast_ctrl_t * multicast_ctrl,struct rdma_cm_event *event)
{
	int ret = 0 ;
	u8 join_state = 1;

	multi_info("START : %s" ,__FUNCTION__);

	multicast_ctrl->pd = ib_alloc_pd(multicast_ctrl->multicast->device,0);
	if(IS_ERR(multicast_ctrl->pd)){
		multi_err("Unable to alloc pd");
		goto out_err;
	}

	ret = rdma_join_multicast(multicast_ctrl->multicast,&multicast_ctrl->multicast_addr,join_state,multicast_ctrl);
	if(ret){
		multi_err("Unable to join multicast\n");
		goto out_join_multicast;
	}	

	return ret;

out_join_multicast:

	ib_dealloc_pd(multicast_ctrl->pd);

out_err:

	return -EINVAL;
}

static int multicast_route_resolved(struct multicast_ctrl_t * multicast_ctrl, struct rdma_cm_event *ev)
{
	int ret = 0;

	multi_info("START : %s" ,__FUNCTION__);

	ret = rdma_join_multicast(multicast_ctrl->multicast,&multicast_ctrl->multicast_addr,0,multicast_ctrl);
	if(ret){
		multi_err("Unable to join multicast\n");
		goto out_multicast_join	;
	}

	return ret;


out_multicast_join:

	return -EINVAL;
}

static int multicast_connect_established(struct multicast_ctrl_t * multicast_ctrl)
{
	multi_info("START : %s" ,__FUNCTION__);

	return 0;
}

static int multicast_ctrl_joined(struct multicast_ctrl_t * multicast_ctrl,struct rdma_cm_event *event)
{

	int ret = 0;
	struct ud_package_t *ud = (struct ud_package_t*)kzalloc(sizeof(*ud),GFP_KERNEL);	
	multi_info("START : %s" ,__FUNCTION__);
	if(!ud){
		multi_err("Unable to kzalloc ud");
		return -EINVAL;
	}

	ud->ah = rdma_create_ah(multicast_ctrl->pd,&event->param.ud.ah_attr,
			RDMA_CREATE_AH_SLEEPABLE);
	if(IS_ERR(ud->ah)){
		multi_err("Unable to create ah");
		kfree(ud);
		return -EINVAL;
	}

	ud->remote_qpn = event->param.ud.qp_num;
	ud->remote_qkey = event->param.ud.qkey;
	multicast_ctrl->ud_package = ud ;

	multicast_ctrl->snd_cq = ib_alloc_cq(multicast_ctrl->multicast->device,multicast_ctrl,4096,
			0,IB_POLL_DIRECT);
	if(IS_ERR(multicast_ctrl->snd_cq)){
		multi_err("Unable to alloc mr");
		goto out_alloc_cq;
	}

	multicast_ctrl->rcv_cq = ib_alloc_cq(multicast_ctrl->multicast->device,multicast_ctrl,4096,
			0,IB_POLL_DIRECT);
	if(IS_ERR(multicast_ctrl->rcv_cq)){
		multi_err("Unable to alloc mr");
		goto out_alloc_cq;
	}

	ret = multicast_ctrl_create_qp(multicast_ctrl);
	if(unlikely(ret)){
		multi_err("Unable to create qp\n");
	}
	ud->qp = multicast_ctrl->multicast->qp;

	return 0;

out_alloc_cq:

	if(multicast_ctrl->rcv_cq)
		ib_free_cq(multicast_ctrl->rcv_cq);
	if(multicast_ctrl->snd_cq)
		ib_free_cq(multicast_ctrl->snd_cq);

	return -EINVAL;
}

static int ib_multicast_cm_handler(struct rdma_cm_id *cm_id,
		struct rdma_cm_event *ev)
{

	struct multicast_ctrl_t *multicast_ctrl = cm_id->context;
	int cm_error = 0;

	pr_info("[MULTICAST]cm_handler msg: %s (%d) status %d id %p\n", rdma_event_msg(ev->event),
			ev->event, ev->status, cm_id);

	switch (ev->event) {
		case RDMA_CM_EVENT_ADDR_RESOLVED:	
			cm_error = multicast_addr_resolved(multicast_ctrl,ev);
			break;
		case RDMA_CM_EVENT_ROUTE_RESOLVED:
			cm_error = multicast_route_resolved(multicast_ctrl,ev);
			break;
		case RDMA_CM_EVENT_ESTABLISHED:
			cm_error = multicast_connect_established(multicast_ctrl);
			break;
		case RDMA_CM_EVENT_REJECTED:
			pr_err("connection rejected\n");
			break;
		case RDMA_CM_EVENT_ADDR_ERROR:
		case RDMA_CM_EVENT_ROUTE_ERROR:
		case RDMA_CM_EVENT_CONNECT_ERROR:
		case RDMA_CM_EVENT_UNREACHABLE:
			pr_err("CM error event %d\n", ev->event);
			cm_error = -ECONNRESET;
			break;
		case RDMA_CM_EVENT_DISCONNECTED:
		case RDMA_CM_EVENT_ADDR_CHANGE:
		case RDMA_CM_EVENT_TIMEWAIT_EXIT:
			pr_err("CM connection closed %d\n", ev->event);
			break;
		case RDMA_CM_EVENT_DEVICE_REMOVAL:
			break;
		case RDMA_CM_EVENT_MULTICAST_JOIN:
			cm_error = multicast_ctrl_joined(multicast_ctrl,ev);
			complete(&multicast_ctrl->cm_done);
			break;
		case RDMA_CM_EVENT_MULTICAST_ERROR:
			pr_err("MULTICAST CM error event");
			cm_error = -ECONNRESET;
			break;
		default:
			pr_err("CM unexpected event: %d\n", ev->event);
			break;
	}

	if (cm_error) {
		multicast_ctrl->cm_error = cm_error;
		complete(&multicast_ctrl->cm_done);
	}

	return 0;
}

static int parse_ipaddr(struct sockaddr_in *saddr, char *ip)
{
	u8 *addr = (u8 *)&saddr->sin_addr.s_addr;
	size_t buflen = strlen(ip);

	pr_info("start: %s\n", __FUNCTION__);

	if (buflen > INET_ADDRSTRLEN){
		return -EINVAL;
	}
	if (in4_pton(ip, -1, addr, '\0', NULL) == 0){
		pr_info("B\n");
		return -EINVAL;
	}
	saddr->sin_family = AF_INET;
	return 0;
}

int init_multicast_ctrl(struct multicast_ctrl_t *multicast_ctrl)
{
	int ret = 0;
	memset(multicast_ctrl,0x0,sizeof(*multicast_ctrl));

	spin_lock_init(&multicast_ctrl->lock);
	init_completion(&multicast_ctrl->cm_done);


	multicast_ctrl->multicast = rdma_create_id(&init_net,ib_multicast_cm_handler,multicast_ctrl,
			RDMA_PS_UDP,IB_QPT_UD);
	if(IS_ERR(multicast_ctrl->multicast)){
		pr_err("Unalbe to create multicast id\n");
		goto out_create_id;
	}

	ret = parse_ipaddr(&multicast_ctrl->multicast_addr_in,multicast_ip);
	if(unlikely(ret)){
		pr_err("Unable to parse ipaddr\n");
		goto out_parse_ipaddr;
	}


	ret = parse_ipaddr(&multicast_ctrl->src_addr_in,src_ip);
	if(unlikely(ret)){
		pr_err("Unable to parse ipaddr\n");
		goto out_parse_ipaddr;
	}	

	ret = rdma_resolve_addr(multicast_ctrl->multicast,&multicast_ctrl->src_addr,
			&multicast_ctrl->multicast_addr,CONNECTION_TIMEOUT_MS);
	if(ret){
		pr_err("Unable to resolve multicast addr\n");
		goto out_parse_ipaddr;
	}

	wait_for_completion_interruptible_timeout(&multicast_ctrl->cm_done,
			msecs_to_jiffies(CONNECTION_TIMEOUT_MS) + 1);
	return ret;


out_parse_ipaddr:

	rdma_destroy_id(multicast_ctrl->multicast);
out_create_id:

	return -EINVAL;
}

static struct rdma_ctrl_t * init_rdma_ctrl(char *ip,int id)
{
	int ret = 0;
	struct rdma_ctrl_t *rdma_ctrl = NULL;

	rdma_ctrl = kzalloc(sizeof(*rdma_ctrl),GFP_KERNEL);
	if(!rdma_ctrl){
		pr_err("Unable to malloc %s \n",id_to_msg(id));
		return NULL;
	}

	rdma_ctrl->id = id;
	init_completion(&rdma_ctrl->cm_done);
	set_rdma(rdma_ctrl);

	pr_info("[%s] : START %s\n",id_to_msg(rdma_ctrl->id),__FUNCTION__);
	pr_info("[%s] : Will connect to %s : %d\n",id_to_msg(rdma_ctrl->id),
			ip,serverport);

	rdma_ctrl->buffer = kzalloc(PAGE_SIZE,GFP_KERNEL);
	if(!rdma_ctrl->buffer){
		pr_err("Unable to alloc rdma buffer\n");
		return NULL;
	}

	rdma_ctrl->rdma = rdma_create_id(&init_net,ib_rdma_cm_handler,rdma_ctrl,
			RDMA_PS_IB,IB_QPT_RC);
	if(IS_ERR(rdma_ctrl)){
		pr_err("Unable to create id \n");
		goto out_create_id;
	}

	ret = parse_ipaddr(&rdma_ctrl->dst_addr_in,ip);
	if(unlikely(ret)){
		pr_err("Unable to parse_addr");
		goto out_alloc_buffer;
	}

	rdma_ctrl->dst_addr_in.sin_port = cpu_to_be16(serverport);

	ret = parse_ipaddr(&rdma_ctrl->src_addr_in,src_ip);
	if(unlikely(ret)){
		pr_err("Unable to parse_addr");
		goto out_alloc_buffer;
	}

	ret = rdma_resolve_addr(rdma_ctrl->rdma,&rdma_ctrl->src_addr,
			&rdma_ctrl->dst_addr,CONNECTION_TIMEOUT_MS);
	if(unlikely(ret)){
		pr_err("Unable to resolve addr\n");
		goto out_alloc_buffer;
	}

	ret = rdma_ctrl_wait_for_cm(rdma_ctrl);
	if(unlikely(ret)){
		pr_err("[%s] Unable to wait cm\n",id_to_msg(rdma_ctrl->id));
	}
	return rdma_ctrl;

out_alloc_buffer:

	rdma_destroy_id(rdma_ctrl->rdma);

out_create_id:

	return NULL;
}


void ib_putback_recv_work(struct ib_qp *qp, struct recv_work *rw)
{
	const struct ib_recv_wr *bad_wr = NULL;
	ib_post_recv(qp, &rw->wr, &bad_wr);
	if(bad_wr){
		pr_err("Unable to put work");
	}
}

static int __ib_rpc(struct ib_qp *qp, uint32_t opcode,
		void *req, size_t req_size, 
		void *res, size_t res_size)
{
	volatile unsigned int done = 0 ;
	int binder = __get_binder();
	struct ib_sge sge = {
		.addr = (uintptr_t)req,
		.length = req_size,
		.lkey = qp->pd->local_dma_lkey ,
	};

	struct ib_send_wr wr = {
		.opcode = IB_WR_SEND_WITH_IMM,
		.send_flags = (IB_SEND_SIGNALED),
		.ex.imm_data = (opcode | binder),
		.num_sge = 1,
		.wr_id = (uintptr_t)&done,
		.sg_list = &sge,
	};

	const struct ib_send_wr *bad_wr = NULL;
	struct recv_work *rw;

	ib_post_send(qp,&wr,&bad_wr);

	if(bad_wr){
		pr_err("Unable to ib send in rpc\n");
		return 1;
	}

	while(!done){
		struct ib_wc wc;
		int nr_completed;
		nr_completed = ib_poll_cq(qp->send_cq,1,&wc);
		if(nr_completed==0){
			continue;
		}else if(nr_completed < 0){
			break;
		}
		if(wc.status != IB_WC_SUCCESS){
			pr_err("work completion status %s\n",ib_wc_status_msg(wc.status));
			return 1;
		}
		*((unsigned int*)wc.wr_id) = 1;
	}
	while(atomic_read(&binders[binder].flags) != 2){
		struct ib_wc wc;
		int bi;
		int nr_completed ;

		nr_completed = ib_poll_cq(qp->recv_cq,1,&wc);
		if(nr_completed == 0 ){
			continue;
		}else if(nr_completed < 0){
			break;
		}

		if(wc.status != IB_WC_SUCCESS){
			pr_err("work completion status %s\n",ib_wc_status_msg(wc.status));
			return 1;
		}
		bi = (wc.ex.imm_data & 0xff);
		binders[bi].private = (void*)wc.wr_id;
		atomic_set(&binders[bi].flags,2);
	}


	rw = (void*)binders[binder].private;
	memcpy(res,rw->buffer,res_size);

	ib_putback_recv_work(qp,rw);
	__put_binder(binder);

	return 0;
}

static int ib_rpc_open_unlocked(struct rdma_ctrl_t *rdma_ctrl, u64 roffset)
{
	int ret = 0;
	int moffset = get_num_memblock(roffset);
	u32 opcode = 0x0;
	struct rdma_info_t *req_rdma_info = NULL;

	if(rdma_ctrl->rdma_info[moffset].rkey != 0){
		goto out_error;
	}

	opcode = (OPCODE_OPEN << 28) | ((roffset >> 20) << 8); 
	req_rdma_info = get_req_rdma_info(rdma_ctrl);

	ret = __ib_rpc(rdma_ctrl->rdma->qp,opcode,(void*)rdma_ctrl->dma_buffer,sizeof(*req_rdma_info),
			&rdma_ctrl->rdma_info[moffset],sizeof(*req_rdma_info));
	if(ret){
		pr_err("Unable to rpc open\n");
		goto out_error;
	}


	memset(req_rdma_info,0x0,sizeof(*req_rdma_info));
	pr_info("[%s][OPEN]: [%d] remote addr : %lx rkey %x  \n",id_to_msg(rdma_ctrl->id),moffset,rdma_ctrl->rdma_info[moffset].remote_addr,
			rdma_ctrl->rdma_info[moffset].rkey);

out_error:

	return ret;
}

int ib_rpc_open(struct rdma_ctrl_t *rdma_ctrl, u64 roffset)
{
	int ret = 0;


	ret = ib_rpc_open_unlocked(rdma_ctrl,roffset);

	return ret;
}

int ib_rpc_open_all(u64 roffset)
{
	struct rdma_ctrl_t *rdma_ctrl = get_main_rdma_ctrl();
	struct rdma_ctrl_t *sub_rdma_ctrl = get_sub_rdma_ctrl();

	int ret = 0;
	int tret = 0 ;

	pr_info("%s",__func__);



	if(sub_rdma_ctrl->alive){
		tret = ib_rpc_open_unlocked(sub_rdma_ctrl,roffset);
		if(unlikely(tret)){
			pr_info("Unable to open main_rdma\n");
		}
	}

	ret |= tret;

	if(rdma_ctrl->alive){
		tret = ib_rpc_open_unlocked(rdma_ctrl,roffset);
		if(unlikely(tret)){
			pr_info("Unable to open main_rdma\n");
		}
	}
	ret |= tret;


	return ret;
}

static void free_addr(struct rdma_ctrl_t *rdma_ctrl)
{
	unsigned long  offset = 0 ;
	void* ret = NULL ;
	xa_for_each(rdma_ctrl->check_list,offset,ret){
		if(ret){
			xa_erase(rdma_ctrl->check_list,offset);
		}
	}
}

static int ib_rpc_commit_unlocked(struct rdma_ctrl_t *rdma_ctrl)
{
	int ret = 0;
	u32 opcode = (OPCODE_COMMIT << 28);
	struct rdma_commit_t *commit =  get_commit_t(rdma_ctrl);
	ret =  __ib_rpc(rdma_ctrl->rdma->qp,opcode,
			(void*)rdma_ctrl->dma_buffer,sizeof(*commit),commit,sizeof(*commit));
	if(ret){
		pr_err("Unable to rpc commit\n");
	}
	return ret;
}

int ib_rpc_commit(struct rdma_ctrl_t *rdma_ctrl)
{
	int ret = 0;
	pr_info("%s",__func__);
	ret = ib_rpc_commit_unlocked(rdma_ctrl);
	free_addr(rdma_ctrl);

	return ret;
}

int ib_rpc_commit_all(void)
{
	struct ctrl_t * ctrl = get_ctrl();
	struct rdma_ctrl_t *rdma_ctrl =  NULL ;
	int ret = 0;
	int tret = 0;
	
	list_for_each_entry(rdma_ctrl,&ctrl->rdma_ctrl_list,list){
		if(rdma_ctrl->alive){
			tret = ib_rpc_commit_unlocked(rdma_ctrl);
			if(unlikely(tret)){
				pr_info("Unable to open main_rdma\n");
			}
		}
	}

	rdma_ctrl = get_main_rdma_ctrl();

	//spin_lock_irq(rdma_ctrl->spinlock);
	free_addr(rdma_ctrl);
	atomic_set(rdma_ctrl->batch,0);
	//spin_unlock_irq(rdma_ctrl->spinlock);

	ret |= tret;
	
	return ret;
}

int __ib_multicast_send(struct ud_package_t *ud, uintptr_t dma, uint32_t header)
{
	int ret = 0;
	volatile unsigned long done = 0;
	const struct ib_send_wr *bad_wr = NULL;

	struct ib_sge sge = {
		.addr = dma,
		.length = PAGE_SIZE,
		.lkey = ud->qp->pd->local_dma_lkey,
	};

	struct ib_ud_wr wr = {
		.wr.next = NULL,
		.wr.wr_id = (uintptr_t)&done,
		.wr.num_sge = 1,
		.wr.sg_list = &sge,
		.wr.opcode = IB_WR_SEND_WITH_IMM,
		.ah = ud->ah,
		.remote_qkey = ud->remote_qkey,
		.remote_qpn = ud->remote_qpn,
		.wr.send_flags =IB_SEND_SIGNALED,
	};

	if(header){
		wr.wr.ex.imm_data = header;
	}

	ret = ib_post_send(ud->qp,&wr.wr,&bad_wr);
	if(bad_wr){
		pr_err("Unable to post multicast send");
		return ret;
	}

	while(!done){

		struct ib_wc wc;
		int nr_completed;
		unsigned long *pdone;
		nr_completed = ib_poll_cq(ud->qp->send_cq,1,&wc);
		if(nr_completed<=0){
			continue;
		}
		if(wc.status != IB_WC_SUCCESS){		
			pr_err("Unable to compelte the send wr : %s\n",ib_wc_status_msg(wc.status));
			return -EINVAL;
		}
		pdone = (long unsigned int *)wc.wr_id;
		*pdone = 1;
	}

	ib_dma_unmap_page(ud->qp->device, dma, PAGE_SIZE, DMA_FROM_DEVICE);
	return ret;
}

static int __ib_rdma_send(struct ib_qp *qp, uintptr_t dma, uint32_t header, 
		enum ib_wr_opcode opcode, struct rdma_info_t *rdma_info, u32 offset)
{
	int ret = 0;
	const struct ib_send_wr *bad_wr;
	volatile unsigned long done = 0;

	struct ib_sge sge = {
		.addr = dma,
		.length = PAGE_SIZE,
		.lkey = qp->pd->local_dma_lkey,
	};

	struct ib_rdma_wr wr = {
		.wr.next = NULL,
		.wr.sg_list = &sge,
		.wr.num_sge =1,
		.wr.send_flags = IB_SEND_SIGNALED,
		.wr.opcode = opcode,
		.wr.wr_id = (uintptr_t)&done,
	};

	if(!(opcode == IB_WR_SEND || opcode == IB_WR_SEND_WITH_IMM)){
		wr.remote_addr = rdma_info->remote_addr + offset;
		wr.rkey = rdma_info->rkey;
	}

	if(header){
		wr.wr.ex.imm_data = header;
	}

	ret = ib_post_send(qp,&wr.wr,&bad_wr);
	if(unlikely(ret)){
		pr_err("Unable to send wr(%d)\n",ret);
	}

	while(!done){
		struct ib_wc wc;
		int nr_completed;
		unsigned long *pdone;
		nr_completed = ib_poll_cq(qp->send_cq,1,&wc);
		if(nr_completed<=0){
			continue;
		}
		if(wc.status != IB_WC_SUCCESS){		
			pr_err("Unable to compelte the send wr : %s\n",ib_wc_status_msg(wc.status));
			return -EINVAL;
		}
		pdone = (long unsigned int *)wc.wr_id;
		*pdone = 1;
	}

	ib_dma_unmap_page(qp->device, dma, PAGE_SIZE, DMA_FROM_DEVICE);

	return ret;
}

static int __init init_ib(void)
{
	int ret = 0;
	struct rdma_ctrl_t *rdma_ctrl = NULL;
	/*
	   u64 count = 0 ;

	   struct page *page; 
	   void *buf = NULL;
	   u64 *rdn = kzalloc(sizeof(*rdn)*1024,GFP_KERNEL);
	   */
	init_ctrl();
	init_binder();
	pr_info("start : %s\n",__FUNCTION__);
	ib_register_client(&ib_client);

	rdma_ctrl = init_rdma_ctrl(fip,0);
	if(!rdma_ctrl){
		pr_err("[%s] Unable to init rdma\n",id_to_msg(rdma_ctrl->id));
		goto out_init_ctrl;
	}
	set_main_rdma_ctrl(rdma_ctrl);

	rdma_ctrl = init_rdma_ctrl(sip,1);
	if(!rdma_ctrl){
		pr_err("[%s] Unable to init rdma\n",id_to_msg(rdma_ctrl->id));
		goto out_init_ctrl;
	}
	
	pr_info("init mbswapib complete!\n");

	return  rdma_ctrl->cm_error;

out_init_ctrl:

	return ret;
}

static void __exit cleanup_ib(void)
{

	struct ctrl_t * ctrl = get_ctrl();	
	struct rdma_ctrl_t *rdma_ctrl = NULL;
	struct rdma_ctrl_t *safe = NULL;
	struct multicast_ctrl_t * multicast_ctrl = get_multicast_ctrl();

	list_for_each_entry_safe(rdma_ctrl,safe,&ctrl->rdma_ctrl_list,list){	
		if(rdma_ctrl->mr)
			ib_dereg_mr(rdma_ctrl->mr);
		if(rdma_ctrl->buffer)
			kfree(rdma_ctrl->buffer);
		if(rdma_ctrl->qp)
			rdma_destroy_qp(rdma_ctrl->rdma);
		if(rdma_ctrl->rdma)
			rdma_destroy_id(rdma_ctrl->rdma);
		list_del_init(&rdma_ctrl->list);
	}

	rdma_leave_multicast(multicast_ctrl->multicast,&multicast_ctrl->multicast_addr);
	if(multicast_ctrl->mr)
		ib_dereg_mr(multicast_ctrl->mr);
	if(multicast_ctrl->qp)
		rdma_destroy_qp(multicast_ctrl->multicast);
	if(multicast_ctrl->multicast)
		rdma_destroy_id(multicast_ctrl->multicast);

	ib_unregister_client(&ib_client);
	return;
}


int maruswap_multicast_write(struct page *page, u64 roffset)
{
	int ret = 0;
	struct ctrl_t *ctrl = get_ctrl();
	struct rdma_ctrl_t *rdma_ctrl = get_main_rdma_ctrl();
	u64 dma = 0;
	u32 offset = get_offset(roffset);
	u32 send_offset = offset & 0x3ffff;
	int nr_memblock = get_num_memblock(roffset);
	unsigned long flags = 0;
	struct rdma_info_t *rdma_info = NULL;

	VM_BUG_ON_PAGE(!PageSwapCache(page), page);

	ret = get_req_for_page(rdma_ctrl->qp->device,&dma,page);
	if(unlikely(ret)){
		pr_err("Unable to get page for dma\n");
		return ret;
	}

	spin_lock_irqsave(rdma_ctrl->spinlock,flags);

	if(check_rdma_info(nr_memblock)){
		ret = ib_rpc_open_all(roffset);
		if(unlikely(ret)){
			pr_err("Unable to rpc open\n");
			goto out_error;
		}
	}

	spin_unlock_irqrestore(rdma_ctrl->spinlock,flags); 

	list_for_each_entry(rdma_ctrl,&ctrl->rdma_ctrl_list,list){
		rdma_info = get_rdma_info(rdma_ctrl,(roffset >>30));
		ret = __ib_rdma_send(rdma_ctrl->rdma->qp,dma,0x0,IB_WR_RDMA_WRITE,
				rdma_info,(send_offset << PAGE_SHIFT));
		if(unlikely(ret)){
			pr_err("Unable to rdma send");
		}
	}

	return ret;

out_error:
	
	spin_unlock_irqrestore(rdma_ctrl->spinlock,flags);
	return 1;
}
EXPORT_SYMBOL(maruswap_multicast_write);

int maruswap_rdma_read(struct page *page, u64 roffset)
{
	int ret = 0;
	struct rdma_ctrl_t *rdma_ctrl = get_main_rdma_ctrl();
	u32 offset = get_offset(roffset);
	u32 send_offset = offset & 0x3ffff;
	u64 dma = 0;
	struct rdma_info_t *rdma_info = NULL;

	rdma_info = get_rdma_info(rdma_ctrl,(roffset >>30));

	VM_BUG_ON_PAGE(!PageSwapCache(page), page);
	VM_BUG_ON_PAGE(!PageLocked(page), page);
	VM_BUG_ON_PAGE(PageUptodate(page), page);

	ret = get_req_for_page(rdma_ctrl->qp->device,&dma,page);
	if(unlikely(ret)){
		pr_err("Unable to get page for dma\n");
		return ret;
	}

	ret = __ib_rdma_send(rdma_ctrl->rdma->qp,dma,0x0,IB_WR_RDMA_READ,
			rdma_info,(send_offset << PAGE_SHIFT));
	if(unlikely(ret)){
		pr_err("Unable to rdma send");
	}

	return ret;

}
EXPORT_SYMBOL(maruswap_rdma_read);


module_init(init_ib);
module_exit(cleanup_ib);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IB_RDMA");
