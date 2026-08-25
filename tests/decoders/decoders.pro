TARGET = decoders_test
include(../common.pri)

SOURCES += \
    DecodersTest.cpp \
    $$SRC_DIR/core/BusMessage.cpp \
    $$SRC_DIR/decoders/UdsDecoder.cpp \
    $$SRC_DIR/decoders/J1939Decoder.cpp \
    $$SRC_DIR/decoders/ProtocolManager.cpp
