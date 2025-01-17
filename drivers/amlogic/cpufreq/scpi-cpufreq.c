// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/scpi_protocol.h>
#include <linux/types.h>

#include "../../cpufreq/arm_big_little.h"

static struct scpi_ops *scpi_ops;
struct scpi_dvfs_info *scpi_dvfs_get_opps(u8 domain);

static struct scpi_dvfs_info *scpi_get_dvfs_info(struct device *cpu_dev)
{
	int domain = topology_physical_package_id(cpu_dev->id);

	if (domain < 0)
		return ERR_PTR(-EINVAL);

	/* TODO: Use API from 3.14 temporary.
	 * When 4.4 arm_mhu and arm_scpi is ported,
	 * this will be replaced by: return scpi_ops->dvfs_get_info(domain);
	 */
	return scpi_dvfs_get_opps(domain);
}

static int scpi_get_transition_latency(struct device *cpu_dev)
{
	struct scpi_dvfs_info *info = scpi_get_dvfs_info(cpu_dev);

	if (IS_ERR(info))
		return PTR_ERR(info);
	return info->latency;
}

static int scpi_init_opp_table(const struct cpumask *cpumask)
{
	int idx, ret;
	struct scpi_opp *opp;
	struct device *cpu_dev = get_cpu_device(cpumask_first(cpumask));
	struct scpi_dvfs_info *info = scpi_get_dvfs_info(cpu_dev);

	if (IS_ERR(info))
		return PTR_ERR(info);

	if (!info->opps)
		return -EIO;

	for (opp = info->opps, idx = 0; idx < info->count; idx++, opp++) {
		ret = dev_pm_opp_add(cpu_dev, opp->freq, opp->m_volt * 1000);
		if (ret) {
			dev_warn(cpu_dev, "failed to add opp %uHz %umV\n",
				 opp->freq, opp->m_volt);
			while (idx-- > 0)
				dev_pm_opp_remove(cpu_dev, (--opp)->freq);
			return ret;
		}
	}

	ret = dev_pm_opp_set_sharing_cpus(cpu_dev, cpumask);
	if (ret)
		dev_err(cpu_dev, "%s: failed to mark OPPs as shared: %d\n",
			__func__, ret);
	return ret;
}

static struct cpufreq_arm_bL_ops scpi_cpufreq_ops = {
	.name	= "scpi",
	.get_transition_latency = scpi_get_transition_latency,
	.init_opp_table = scpi_init_opp_table,
	.free_opp_table = dev_pm_opp_cpumask_remove_table,
};

static int scpi_cpufreq_probe(struct platform_device *pdev)
{
	/* TODO:  TODO: Use API from 3.14 temporary.
	 * When 4.4 arm_mhu and arm_scpi is ported, these lines should be added:
	 * scpi_ops = get_scpi_ops();
	 * if (!scpi_ops)
	 *	return -EIO;
	 */
	pr_info("[%s %d]\n", __func__, __LINE__);
	return bL_cpufreq_register(&scpi_cpufreq_ops);
}

static int scpi_cpufreq_remove(struct platform_device *pdev)
{
	bL_cpufreq_unregister(&scpi_cpufreq_ops);
	scpi_ops = NULL;
	return 0;
}

static struct platform_driver scpi_cpufreq_platdrv = {
	.driver = {
		.name	= "scpi-cpufreq",
	},
	.probe		= scpi_cpufreq_probe,
	.remove		= scpi_cpufreq_remove,
};
module_platform_driver(scpi_cpufreq_platdrv);

MODULE_AUTHOR("Sudeep Holla <sudeep.holla@arm.com>");
MODULE_DESCRIPTION("ARM SCPI CPUFreq interface driver");
MODULE_LICENSE("GPL v2");
