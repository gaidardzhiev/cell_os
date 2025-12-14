#E5: Org Pack v1
ORG_SRCS ?= $(ORG_COMMON_SRCS)
ORG_HDRS ?= $(ORG_COMMON_HDRS)
ORG_OBJS ?= $(ORG_COMMON_OBJS)
MITO_OBJ := $(BUILD_DIR)/genes/mito_energy.o
GOLGI_OBJ := $(BUILD_DIR)/genes/golgi_route.o
LYSO_OBJ := $(BUILD_DIR)/genes/lyso_cleanup.o
PEROXI_OBJ := $(BUILD_DIR)/genes/peroxi_sanitize.o

$(BUILD_DIR)/abi_e5_check.o: src/core/abi_e5_check.c $(ORG_HDRS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -Iinclude -c $< -o $@

.PHONY: organelles
organelles: $(ORG_OBJS) $(BUILD_DIR)/organelles.o $(BUILD_DIR)/channel_graph.o $(BUILD_DIR)/abi_e5_check.o

TEST_ORG := build/test_mito build/test_golgi build/test_lyso build/test_peroxi

build/test_mito: tests/test_mito.c $(ORG_HDRS) $(MITO_OBJ) $(PARCEL_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_mito.c $(MITO_OBJ) $(PARCEL_OBJ) -o $@

build/test_golgi: tests/test_golgi.c $(ORG_HDRS) $(GOLGI_OBJ) $(BUILD_DIR)/channel_graph.o include/core/channel.h $(PARCEL_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_golgi.c $(GOLGI_OBJ) $(BUILD_DIR)/channel_graph.o $(PARCEL_OBJ) -o $@

build/test_lyso: tests/test_lyso.c $(ORG_HDRS) $(LYSO_OBJ) $(PARCEL_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_lyso.c $(LYSO_OBJ) $(PARCEL_OBJ) -o $@

build/test_peroxi: tests/test_peroxi.c $(ORG_HDRS) $(PEROXI_OBJ) $(PARCEL_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_peroxi.c $(PEROXI_OBJ) $(PARCEL_OBJ) -o $@

.PHONY: test-org
test-org: $(TEST_ORG)
	@set -e; for t in $(TEST_ORG); do $$t; done; echo "#E5 ORG ok"

.PHONY: e5
e5: organelles test-org
