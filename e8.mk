#E8 Observability v2 (core) ???
OBS_OBJS := build/obs.o build/obs_trace.o

build/obs.o: src/core/obs.c include/core/obs.h include/abi/msg.h include/core/parcel.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c src/core/obs.c -o $@

build/obs_trace.o: src/core/obs_trace.c include/core/obs_trace.h include/core/obs.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude -c src/core/obs_trace.c -o $@

.PHONY: obs
obs: $(OBS_OBJS)

REPLAY_BIN := build/obs_replay
INDEX_BIN := build/obs_index
TEST_OBS_EQ := build/test_obs_replay_equiv
TEST_OBS_FR := build/test_obs_frames

$(REPLAY_BIN): tools/obs_replay.c src/core/obs.c src/core/obs_trace.c libparcel/parcel.c \
	include/core/obs.h include/core/obs_trace.h include/core/parcel.h include/abi/msg.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tools/obs_replay.c src/core/obs.c src/core/obs_trace.c libparcel/parcel.c -o $@

$(INDEX_BIN): tools/obs_index.c src/core/obs_trace.c include/core/obs_trace.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tools/obs_index.c src/core/obs_trace.c -o $@

$(TEST_OBS_EQ): tests/test_obs_replay_equiv.c src/core/obs.c libparcel/parcel.c \
	include/core/obs.h include/core/parcel.h include/abi/msg.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_obs_replay_equiv.c src/core/obs.c libparcel/parcel.c -o $@

$(TEST_OBS_FR): tests/test_obs_frames.c src/core/obs.c src/core/obs_trace.c libparcel/parcel.c \
	include/core/obs.h include/core/obs_trace.h include/core/parcel.h include/abi/msg.h
	@mkdir -p build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iinclude tests/test_obs_frames.c src/core/obs.c src/core/obs_trace.c libparcel/parcel.c -o $@

.PHONY: replay test-obs
replay: $(REPLAY_BIN)

.PHONY: index
index: $(INDEX_BIN)

test-obs: $(TEST_OBS_EQ) $(TEST_OBS_FR) $(REPLAY_BIN)
	@./$(TEST_OBS_EQ)
	@./$(TEST_OBS_FR) | ./$(REPLAY_BIN) >/dev/null 2>/dev/null
	@echo "#E8 OBS v2 replay=ok"
