#include "inc/uds/uds_core.h"
#include <assert.h>
#include <stdint.h>

int main(void) {
    // Test SID values match ISO spec
    assert(DIAGNOSTIC_SESSION_CONTROL == 0x10);
    assert(ECU_RESET == 0x11);
    assert(CLEAR_DIAGNOSTIC_INFO == 0x14);
    assert(READ_DTC_INFO == 0x19);
    assert(READ_DATA_BY_IDENTIFIER == 0x22);
    assert(READ_MEMORY_BY_ADDRESS == 0x23);
    assert(READ_SCALING_DATA_BY_ID == 0x24);
    assert(SECURITY_ACCESS == 0x27);
    assert(COMMUNICATION_CONTROL == 0x28);
    assert(AUTHENTICATION == 0x29);
    assert(READ_DATA_BY_PERIODIC_ID == 0x2A);
    assert(DYNAMICALLY_DEFINE_DATA_ID == 0x2C);
    assert(WRITE_DATA_BY_IDENTIFIER == 0x2E);
    assert(INPUT_OUTPUT_CONTROL_BY_ID == 0x2F);
    assert(ROUTINE_CONTROL == 0x31);
    assert(REQUEST_DOWNLOAD == 0x34);
    assert(REQUEST_UPLOAD == 0x35);
    assert(TRANSFER_DATA == 0x36);
    assert(REQUEST_TRANSFER_EXIT == 0x37);
    assert(REQUEST_FILE_TRANSFER == 0x38);
    assert(WRITE_MEMORY_BY_ADDRESS == 0x3D);
    assert(TESTER_PRESENT == 0x3E);
    assert(SECURED_DATA_TRANSMISSION == 0x84);
    assert(CONTROL_DTC_SETTING == 0x85);
    assert(RESPONSE_ON_EVENT == 0x86);
    assert(LINK_CONTROL == 0x87);
    
    // Test response SIDs = request SID + 0x40
    assert(DIAGNOSTIC_SESSION_CONTROL_RSP == (DIAGNOSTIC_SESSION_CONTROL + 0x40));
    assert(ECU_RESET_RSP == (ECU_RESET + 0x40));
    assert(CLEAR_DIAGNOSTIC_INFO_RSP == (CLEAR_DIAGNOSTIC_INFO + 0x40));
    assert(READ_DTC_INFO_RSP == (READ_DTC_INFO + 0x40));
    assert(READ_DATA_BY_IDENTIFIER_RSP == (READ_DATA_BY_IDENTIFIER + 0x40));
    assert(READ_MEMORY_BY_ADDRESS_RSP == (READ_MEMORY_BY_ADDRESS + 0x40));
    assert(READ_SCALING_DATA_BY_ID_RSP == (READ_SCALING_DATA_BY_ID + 0x40));
    assert(SECURITY_ACCESS_RSP == (SECURITY_ACCESS + 0x40));
    assert(COMMUNICATION_CONTROL_RSP == (COMMUNICATION_CONTROL + 0x40));
    assert(AUTHENTICATION_RSP == (AUTHENTICATION + 0x40));
    assert(READ_DATA_BY_PERIODIC_ID_RSP == (READ_DATA_BY_PERIODIC_ID + 0x40));
    assert(DYNAMICALLY_DEFINE_DATA_ID_RSP == (DYNAMICALLY_DEFINE_DATA_ID + 0x40));
    assert(WRITE_DATA_BY_IDENTIFIER_RSP == (WRITE_DATA_BY_IDENTIFIER + 0x40));
    assert(INPUT_OUTPUT_CONTROL_BY_ID_RSP == (INPUT_OUTPUT_CONTROL_BY_ID + 0x40));
    assert(ROUTINE_CONTROL_RSP == (ROUTINE_CONTROL + 0x40));
    assert(REQUEST_DOWNLOAD_RSP == (REQUEST_DOWNLOAD + 0x40));
    assert(REQUEST_UPLOAD_RSP == (REQUEST_UPLOAD + 0x40));
    assert(TRANSFER_DATA_RSP == (TRANSFER_DATA + 0x40));
    assert(REQUEST_TRANSFER_EXIT_RSP == (REQUEST_TRANSFER_EXIT + 0x40));
    assert(REQUEST_FILE_TRANSFER_RSP == (REQUEST_FILE_TRANSFER + 0x40));
    assert(WRITE_MEMORY_BY_ADDRESS_RSP == (WRITE_MEMORY_BY_ADDRESS + 0x40));
    assert(TESTER_PRESENT_RSP == (TESTER_PRESENT + 0x40));
    assert(SECURED_DATA_TRANSMISSION_RSP == (SECURED_DATA_TRANSMISSION + 0x40));
    assert(CONTROL_DTC_SETTING_RSP == (CONTROL_DTC_SETTING + 0x40));
    assert(RESPONSE_ON_EVENT_RSP == (RESPONSE_ON_EVENT + 0x40));
    assert(LINK_CONTROL_RSP == (LINK_CONTROL + 0x40));
    
    // Test NRC values - check a few key ones
    assert(NRC_GENERAL_REJECT == 0x10);
    assert(NRC_SERVICE_NOT_SUPPORTED == 0x11);
    assert(NRC_SUB_FUNCTION_NOT_SUPPORTED == 0x12);
    assert(NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT == 0x13);
    assert(NRC_CONDITIONS_NOT_CORRECT == 0x22);
    assert(NRC_REQUEST_SEQUENCE_ERROR == 0x24);
    assert(NRC_REQUEST_OUT_OF_RANGE == 0x31);
    assert(NRC_SECURITY_ACCESS_DENIED == 0x33);
    assert(NRC_INVALID_KEY == 0x35);
    assert(NRC_EXCEED_NUMBER_OF_ATTEMPTS == 0x36);
    assert(NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED == 0x37);
    assert(NRC_REQUEST_CORRECTLY_RECEIVED_RESPONSE_PENDING == 0x78);
    assert(NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION == 0x7E);
    assert(NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION == 0x7F);
    assert(NRC_RPM_TOO_HIGH == 0x81);
    assert(NRC_VOLTAGE_TOO_HIGH == 0x92);
    assert(NRC_VOLTAGE_TOO_LOW == 0x93);
    assert(NRC_RESOURCE_TEMPORARILY_NOT_AVAILABLE == 0x94);
    
    // Test helper macros
    assert(IS_REQUEST_SID(DIAGNOSTIC_SESSION_CONTROL) == 1);
    assert(IS_REQUEST_SID(ECU_RESET) == 1);
    assert(IS_REQUEST_SID(READ_DATA_BY_IDENTIFIER) == 1);
    assert(IS_REQUEST_SID(SECURITY_ACCESS) == 1);
    assert(IS_REQUEST_SID(REQUEST_DOWNLOAD) == 1);
    assert(IS_REQUEST_SID(TESTER_PRESENT) == 1);
    assert(IS_REQUEST_SID(CONTROL_DTC_SETTING) == 1);
    assert(IS_REQUEST_SID(0x0F) == 0); // Below range
    assert(IS_REQUEST_SID(0x3F) == 0); // Above first range
    assert(IS_REQUEST_SID(0x83) == 0); // Between ranges
    assert(IS_REQUEST_SID(0x88) == 0); // Above second range
    
    assert(IS_RESPONSE_SID(DIAGNOSTIC_SESSION_CONTROL_RSP) == 1);
    assert(IS_RESPONSE_SID(ECU_RESET_RSP) == 1);
    assert(IS_RESPONSE_SID(READ_DATA_BY_IDENTIFIER_RSP) == 1);
    assert(IS_RESPONSE_SID(SECURITY_ACCESS_RSP) == 1);
    assert(IS_RESPONSE_SID(REQUEST_DOWNLOAD_RSP) == 1);
    assert(IS_RESPONSE_SID(TESTER_PRESENT_RSP) == 1);
    assert(IS_RESPONSE_SID(CONTROL_DTC_SETTING_RSP) == 1);
    assert(IS_RESPONSE_SID(0x4F) == 0); // Below range
    assert(IS_RESPONSE_SID(0x7F) == 0); // Above first range
    assert(IS_RESPONSE_SID(0xC3) == 0); // Between ranges
    assert(IS_RESPONSE_SID(0xC8) == 0); // Above second range
    
    assert(SID_TO_RESPONSE(DIAGNOSTIC_SESSION_CONTROL) == DIAGNOSTIC_SESSION_CONTROL_RSP);
    assert(SID_TO_RESPONSE(ECU_RESET) == ECU_RESET_RSP);
    assert(SID_TO_RESPONSE(READ_DATA_BY_IDENTIFIER) == READ_DATA_BY_IDENTIFIER_RSP);
    assert(SID_FROM_RESPONSE(DIAGNOSTIC_SESSION_CONTROL_RSP) == DIAGNOSTIC_SESSION_CONTROL);
    assert(SID_FROM_RESPONSE(ECU_RESET_RSP) == ECU_RESET);
    assert(SID_FROM_RESPONSE(READ_DATA_BY_IDENTIFIER_RSP) == READ_DATA_BY_IDENTIFIER);
    
    // Test subfunction struct
    uds_subfunction_t sf;
    sf.value = 0x3F; // 7 bits max
    sf.suppress_rsp = 1;
    assert(sf.value == 0x3F);
    assert(sf.suppress_rsp == 1);
    
    // Test session enum
    assert(UDS_DEFAULT_SESSION == 0x01);
    assert(UDS_PROGRAMMING_SESSION == 0x02);
    assert(UDS_EXTENDED_SESSION == 0x03);
    
    // Test addressing enum
    assert(UDS_PHYSICAL == 0);
    assert(UDS_FUNCTIONAL == 1);
    
    // Test std return struct
    uds_std_return_t std_return;
    std_return.p2_server_max = 0x1000;
    std_return.p2_star_server_max = 0x2000;
    assert(std_return.p2_server_max == 0x1000);
    assert(std_return.p2_star_server_max == 0x2000);
    
    // Test msg struct
    uint8_t test_data[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uds_msg_t msg;
    msg.ta_type = UDS_PHYSICAL;
    msg.nad = 0x01;
    msg.data = test_data;
    msg.len = 5;
    assert(msg.ta_type == UDS_PHYSICAL);
    assert(msg.nad == 0x01);
    assert(msg.data == test_data);
    assert(msg.len == 5);
    
    return 0;
}