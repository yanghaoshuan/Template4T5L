#include "v851_protocol.h"

#if v851PROTOCOL_ENABLED

#include <string.h>

static V851BootstrapResult xdata v851_bootstrap_result;
static uint8_t v851_bootstrap_result_valid;

static void V851BootstrapCopyString(char *destination,
                                    uint16_t capacity,
                                    const V851TlvField *field)
{
    uint16_t length;

    length = field->length;
    if(length >= capacity)
    {
        length = (uint16_t)(capacity - 1U);
    }
    if(length != 0U)
    {
        memcpy(destination, field->value, length);
    }
    destination[length] = '\0';
}

static void V851BootstrapApplySegment(const V851TlvSegment *segment)
{
    V851TlvFieldCursor cursor;
    V851TlvField field;
    V851TlvIterResult result;

    /* Validate again locally so this hook remains atomic if called directly. */
    V851TlvFieldCursorInit(&cursor, segment->payload, segment->length);
    for(;;)
    {
        result = V851TlvFieldNext(&cursor, &field);
        if(result == V851_TLV_ITER_END)
        {
            break;
        }
        if(result != V851_TLV_ITER_FIELD)
        {
            return;
        }
    }

    memset(&v851_bootstrap_result, 0, sizeof(v851_bootstrap_result));
    v851_bootstrap_result.struct_type = V851_TLV_STRUCT_BOOTSTRAP_RESULT;
    V851TlvFieldCursorInit(&cursor, segment->payload, segment->length);
    for(;;)
    {
        result = V851TlvFieldNext(&cursor, &field);
        if(result == V851_TLV_ITER_END)
        {
            v851_bootstrap_result_valid = 1U;
            return;
        }
        if(result != V851_TLV_ITER_FIELD)
        {
            return;
        }
        switch(field.tag)
        {
            case V851_TLV_TAG_BOOT_DEVICE_SN:
                V851BootstrapCopyString(v851_bootstrap_result.device_sn,
                                        V851_BOOTSTRAP_DEVICE_SN_SIZE, &field);
                break;
            case V851_TLV_TAG_BOOT_BLE_ID:
                V851BootstrapCopyString(v851_bootstrap_result.ble_id,
                                        V851_BOOTSTRAP_BLE_ID_SIZE, &field);
                break;
            case V851_TLV_TAG_BOOT_API_ENDPOINT:
                V851BootstrapCopyString(v851_bootstrap_result.api_endpoint,
                                        V851_BOOTSTRAP_API_ENDPOINT_SIZE, &field);
                break;
            case V851_TLV_TAG_BOOT_BIND_STATUS:
                V851BootstrapCopyString(v851_bootstrap_result.bind_status,
                                        V851_BOOTSTRAP_BIND_STATUS_SIZE, &field);
                break;
            case V851_TLV_TAG_BOOT_QR_URL:
                V851BootstrapCopyString(v851_bootstrap_result.qr_url,
                                        V851_BOOTSTRAP_QR_URL_SIZE, &field);
                break;
            default:
                break;
        }
    }
}

const V851BootstrapResult *V851BootstrapResultGet(void)
{
    if(v851_bootstrap_result_valid == 0U)
    {
        return NULL;
    }
    return &v851_bootstrap_result;
}

/* Single project hook for Bootstrap and structures without a DGUS mapping. */
void V851TlvApplicationSegment(uint8_t command,
                               const V851TlvSegment *segment)
{
    if((command == V851_TLV_CMD_BOOTSTRAP_RESULT) &&
       (segment != NULL) &&
       (segment->struct_type == V851_TLV_STRUCT_BOOTSTRAP_RESULT))
    {
        V851BootstrapApplySegment(segment);
    }
}

#endif /* v851PROTOCOL_ENABLED */
