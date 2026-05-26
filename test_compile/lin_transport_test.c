#include "inc/uds/uds_lin_transport.h"
#include <assert.h>
#include <stdint.h>

int main(void) {
    // Test LIN frame structure layout
    assert(sizeof(lin_frame_t) == LIN_FRAME_SIZE);
    assert(LIN_FRAME_SIZE == 8);
    assert(LIN_SF_MAX_LEN == 6);
    assert(LIN_FF_MAX_LEN == 4095);
    
    // Test PCI encoding constants
    assert(LIN_PCI_SF == 0x00);
    assert(LIN_PCI_FF == 0x20);
    assert(LIN_PCI_CF == 0x40);
    assert(LIN_PCI_MASK == 0xE0);
    assert(LIN_PCI_LEN_MASK == 0x1F);
    
    // Test diagnostic frame ID constants
    assert(LIN_DIAG_REQUEST_ID == 0x3C);
    assert(LIN_DIAG_RESPONSE_ID == 0x3D);
    
    // Test default constants
    assert(LIN_BAUDRATE == 19200);
    assert(LIN_NAD_DEFAULT == 0x01);
    
    // Test struct member offsets (compile-time check)
    lin_frame_t frame;
    lin_diag_pdu_t pdu;
    
    // These should compile without error - just checking member access
    frame.data[0] = 0x00;
    pdu.nad = 0x01;
    pdu.pci = 0x02;
    pdu.uds_data = NULL;
    pdu.data_len = 0x00;
    
    return 0;
}