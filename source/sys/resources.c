#include "sys/resources.h"

static void thread_exit_shell() { 
	thread_exit();
}

static void decompose_res (res_type_t res) {
	kprint ("\tRESOURCE\r\n");
	kprint ("|0x%x|0x%x|0x%x|0x%x|\r\n", (res >> 24) & 0xff,
										 (res >> 16) & 0xff,
										 (res >> 8)  & 0xff,
										 (res)  & 0xff);
}

static int res_is_valid (res_type_t res) {
	return 	(((res)      & 0xff) <= RES_FIRST_TYPE_MAX &&
			((res >> 8)  & 0xff) <= RES_SEC_TYPE_MAX   &&
			((res >> 16) & 0xff) <= RES_THIRD_TYPE_MAX &&
			((res >> 24) & 0xff) <= RES_FIRST_TYPE_MAX);
}

static int find_res_in_table_by_type (res_unit* unit, res_type_t res) {
	return 0;
}

int res_get (void* data, res_type_t res, pid_t src, sflag_t flag) {
	
	pid_t from;
	int rd;
	res_unit* ures;

	if (res_is_valid (res) == 0) {
#ifdef DEBUG
		kprint ("Unknown resource\r\n");
#endif
		return 0;
	}

#ifdef DEBUG
	decompose_res (res);
#endif

	rd = find_res_in_table_by_type (ures, res);

	if ((flag & R_WAITFROM) && (flag & R_NONBLOCK)) {
#ifdef DEBUG
		kprint ("Can't get resources with WAITFROM and NONBLOCK in the same time\r\n");
#endif
		return -1;
	}

	if (flag & R_WAITFROM)
		from = src;
	else 
		from = GET_KERNEL_THREAD()->pid;

	if (from == cur_thread->pid) {
		if (ures->pid == from)
			return rd;
		else 
			return -1;
	}

	if ((flag & R_WAITFROM) && from != 0) {
		cur_thread->state = BLOCKED;
		thread_exit_shell();
	} else {
		while (ures->pid != from) {
			if ((flag & R_NONBLOCK) == 0) {
				cur_thread->state = BLOCKED;
				thread_exit_shell();
			} else {
#ifdef DEBUG
				kprint ("Unavaliable resource\r\n");
#endif
				return -1;
			}
		}
	}
	ures->pid = cur_thread->pid;
	return rd;
}


int res_give (int rd, pid_t dest, sflag_t flag) {

	res_unit* ures;
	pid_t to;
	int retv;

	if (find_res_in_table_by_rd (ures, rd) == 0) {
#ifdef DEBUG
		kprint ("Unknown resource\r\n");
#endif
		return -1;
	}

	if (ures->pid == cur_thread->pid) {
#ifdef DEBUG
		kprint ("Unavaliable resource\r\n");
#endif
		return -1;
	}

	if (flag & R_SENDTO) 
		to = dest;
	else
		to = GET_KERNEL_THREAD()->pid;

	kthread* receiver = find_thread_pid (to);
	
	if (receiver == NULL) {
#ifdef DEBUG
		kprint ("There are no thread with %d pid\r\n", to);
#endif
		return -1;
	}

	do {
		retv = find_msg (GET_KERNEL_THREAD()->buffer, MSG_GET_TYPE, ures->type, to);
		if (retv == 0) {
			cur_thread->state = BLOCKED;
			thread_exit_shell();
		}
	} while (retv == 0);
	
	if (receiver->state == BLOCKED) {
		receiver->state = WAIT;
		active_add_tail (th_active_head, receiver);
	}

	return 0;
}
