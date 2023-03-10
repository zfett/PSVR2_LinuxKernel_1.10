/*
 * drivers/staging/android/ion/mtk/gpu_sram_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "../ion.h"
#include "../ion_priv.h"
#include "gpu_sram_heap.h"

struct ion_gpu_sram_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
};

ion_phys_addr_t ion_gpu_sram_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align)
{
	struct ion_gpu_sram_heap *gpu_sram_heap =
		container_of(heap, struct ion_gpu_sram_heap, heap);
	unsigned long offset = gen_pool_alloc(gpu_sram_heap->pool, size);

	if (!offset)
		return ION_CARVEOUT_ALLOCATE_FAIL;

	return offset;
}

void ion_gpu_sram_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size)
{
	struct ion_gpu_sram_heap *gpu_sram_heap =
		container_of(heap, struct ion_gpu_sram_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(gpu_sram_heap->pool, addr, size);
}

static int ion_gpu_sram_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  ion_phys_addr_t *addr, size_t *len)
{
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	*addr = paddr;
	*len = buffer->size;
	return 0;
}

static int ion_gpu_sram_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	struct sg_table *table;
	ion_phys_addr_t paddr;
	int ret;

	if (align > PAGE_SIZE)
		return -EINVAL;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr = ion_gpu_sram_allocate(heap, size, align);
	if (paddr == ION_CARVEOUT_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->priv_virt = table;

	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_gpu_sram_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	ion_heap_buffer_zero(buffer);

/*
*	if (ion_buffer_cached(buffer))
*		dma_sync_sg_for_device(g_ion_device->dev.this_device,
*				       table->sgl,
*				       table->nents,
*				       DMA_BIDIRECTIONAL);
*/
	ion_gpu_sram_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct sg_table *ion_gpu_sram_heap_map_dma(struct ion_heap *heap,
						  struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

static void ion_gpu_sram_heap_unmap_dma(struct ion_heap *heap,
					struct ion_buffer *buffer)
{
}

static struct ion_heap_ops gpu_sram_heap_ops = {
	.allocate = ion_gpu_sram_heap_allocate,
	.free = ion_gpu_sram_heap_free,
	.phys = ion_gpu_sram_heap_phys,
	.map_dma = ion_gpu_sram_heap_map_dma,
	.unmap_dma = ion_gpu_sram_heap_unmap_dma,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_gpu_sram_heap_create(
					struct ion_platform_heap *heap_data)
{
	struct ion_gpu_sram_heap *gpu_sram_heap;
/*	int ret; */

	struct page *page;
	size_t size;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;
/*
*	ion_pages_sync_for_device(g_ion_device->dev.this_device,
*				  page, size,
*				  DMA_BIDIRECTIONAL);
*/
/*	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
*	if (ret)
*		return ERR_PTR(ret);
*/
	gpu_sram_heap = kzalloc(sizeof(*gpu_sram_heap), GFP_KERNEL);
	if (!gpu_sram_heap)
		return ERR_PTR(-ENOMEM);

	gpu_sram_heap->pool = gen_pool_create(12, -1);
	if (!gpu_sram_heap->pool) {
		kfree(gpu_sram_heap);
		return ERR_PTR(-ENOMEM);
	}
	gpu_sram_heap->base = heap_data->base;
	gen_pool_add(gpu_sram_heap->pool, gpu_sram_heap->base, heap_data->size,
		     -1);
	gpu_sram_heap->heap.ops = &gpu_sram_heap_ops;
	gpu_sram_heap->heap.type = ION_HEAP_TYPE_GPU_SRAM;
	gpu_sram_heap->heap.flags = 0; //ION_HEAP_FLAG_DEFER_FREE;

	return &gpu_sram_heap->heap;
}

void ion_gpu_sram_heap_destroy(struct ion_heap *heap)
{
	struct ion_gpu_sram_heap *gpu_sram_heap =
	     container_of(heap, struct  ion_gpu_sram_heap, heap);

	gen_pool_destroy(gpu_sram_heap->pool);
	kfree(gpu_sram_heap);
	gpu_sram_heap = NULL;
}

