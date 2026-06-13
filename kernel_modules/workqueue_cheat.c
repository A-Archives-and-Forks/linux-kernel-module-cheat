/* https://cirosantilli.com/linux-kernel-module-cheat#workqueues */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h> /* usleep_range */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h> /* atomic_t */
#include <linux/workqueue.h>

struct my_work {
	struct work_struct work;
	int queue_id;
	int item_id;
};

static int nqueues = 2;
module_param(nqueues, int, 0444);
MODULE_PARM_DESC(nqueues, "Number of workqueues to spawn (>=1)");

static int nworks = 1;
module_param(nworks, int, 0444);
MODULE_PARM_DESC(nworks, "Number of work items per queue (>=1)");

static struct workqueue_struct **queues;
static struct my_work *works;
static atomic_t run = ATOMIC_INIT(1);

static void work_func(struct work_struct *work)
{
	struct my_work *mw = container_of(work, struct my_work, work);
	int i = 0;
	unsigned int sleep_us = 500000 * mw->queue_id; /* queue 1 => 0.5s, queue 2 => 1s */

	while (atomic_read(&run)) {
		pr_info("queue_id=%d item_id=%d i=%d\n", mw->queue_id, mw->item_id, i);
		usleep_range(sleep_us, sleep_us + 1000);
		i++;
		if (i == 5)
			break;
	}
}

static int myinit(void)
{
	int i, j, idx = 0;

	if (nqueues < 1 || nworks < 1)
		return -EINVAL;
	queues = kcalloc(nqueues, sizeof(*queues), GFP_KERNEL);
	if (!queues)
		return -ENOMEM;
	works = kcalloc(nqueues * nworks, sizeof(*works), GFP_KERNEL);
	if (!works) {
		kfree(queues);
		return -ENOMEM;
	}
	for (i = 0; i < nqueues; i++) {
		struct workqueue_struct *wq;

		wq = alloc_workqueue("myworkqueue-%d", WQ_UNBOUND, 1, i + 1);
		if (!wq)
			goto err_create;
		queues[i] = wq;

		for (j = 0; j < nworks; j++) {
			struct my_work *mw = &works[idx++];

			mw->queue_id = i + 1;
			mw->item_id = j + 1;
			INIT_WORK(&mw->work, work_func);
			queue_work(queues[i], &mw->work);
		}
	}

	return 0;

err_create:
	while (--i >= 0)
		destroy_workqueue(queues[i]);
	kfree(works);
	kfree(queues);
	return -ENOMEM;
}

static void myexit(void)
{
	int i;

	atomic_set(&run, 0);
	if (queues) {
		for (i = 0; i < nqueues; i++) {
			if (!queues[i])
				continue;
			flush_workqueue(queues[i]);
			destroy_workqueue(queues[i]);
		}
	}
	kfree(works);
	kfree(queues);
}

module_init(myinit)
module_exit(myexit)
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(__FILE__);
