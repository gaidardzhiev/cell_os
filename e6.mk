# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
# Licensed under the MIT License. See LICENSE in project root.
# E6: Interrupt > Event Bridge

IRQ_OBJS := build/irq_bridge.o build/events.o
TEST_IRQ := build/test_irq_flood build/test_irq_order

build/irq_bridge.o: src/core/irq_bridge.c include/core/irq_bridge.h include/core/events.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

build/events.o: src/core/events.c include/core/events.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

build/test_irq_flood: tests/test_irq_flood.c $(IRQ_OBJS)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_irq_flood.c $(IRQ_OBJS) -o $@

build/test_irq_order: tests/test_irq_order.c $(IRQ_OBJS)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_irq_order.c $(IRQ_OBJS) -o $@

.PHONY: irq test-irq
irq: $(IRQ_OBJS)

test-irq: $(TEST_IRQ)
	@set -e; for t in $(TEST_IRQ); do $$t; done; echo "#E6 EVT bridge ok"
