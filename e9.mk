# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
# Licensed under the MIT License. See LICENSE in project root.
#E9 Security (MAC + Trust + Fuzz) ???

SEC_OBJS := $(BUILD_DIR)/crypto.o $(BUILD_DIR)/trust.o
TEST_MAC := $(BUILD_DIR)/test_mac
TEST_TRUST := $(BUILD_DIR)/test_trust
FUZZ := $(BUILD_DIR)/fuzz_parsers

$(BUILD_DIR)/crypto.o: src/core/crypto.c include/core/crypto.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

$(BUILD_DIR)/trust.o: src/core/trust.c include/core/trust.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

$(TEST_MAC): tests/test_mac.c include/core/crypto.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude $< -o $@

$(TEST_TRUST): tests/test_trust.c include/core/trust.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude $< -o $@

$(FUZZ): tests/fuzz_parsers.c src/core/obs.c src/core/obs_trace.c libparcel/parcel.c \
		 include/core/obs.h include/core/obs_trace.h include/core/parcel.h include/abi/msg.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude $^ -o $@

.PHONY: security test-sec
security: $(SEC_OBJS)

test-sec: $(TEST_MAC) $(TEST_TRUST) $(FUZZ)
	@$(TEST_MAC)
	@$(TEST_TRUST)
	@$(FUZZ)
	@echo "#E9 SEC mac+roots"
