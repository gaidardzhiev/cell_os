#E7: Update/Rollback v2 + Migration Gene
UPDATE_OBJ := build/update.o
TEST_UPDATE := build/test_update
TEST_MIGRATE := build/test_migrate

build/update.o: src/core/update.c include/core/update.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

build/test_update: tests/test_update.c src/core/update.c include/core/update.h tools/mock_storage.h $(HASH_OBJS)
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -I. tests/test_update.c src/core/update.c $(HASH_OBJS) -o $@

build/test_migrate: tests/test_migrate.c src/genes/migrate.c include/gene/migrate.h include/gene/organelle.h include/core/parcel.h include/abi/msg.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -I. tests/test_migrate.c src/genes/migrate.c libparcel/parcel.c -o $@

.PHONY: update test-update
update: $(UPDATE_OBJ)

test-update: build/test_update build/test_migrate
	@build/test_update
	@build/test_migrate
	@echo "#E7 UPDATE ready"

HASH_OBJS := build/hash_stub.o
ifdef BUILD_STRICT_CRYPTO
CFLAGS += -DBUILD_STRICT_CRYPTO=1
HASH_OBJS += build/hash_sha256.o
endif

build/hash_stub.o: src/core/hash_stub.c include/core/hash.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

build/hash_sha256.o: src/core/hash_sha256.c include/core/hash.h
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c $< -o $@

build/update.o: $(HASH_OBJS)
build/test_update: $(HASH_OBJS)
