// ----------------------------------------------------------------------------
/*!
 * @file		util.h
 * @author    	John Steggall
 * @date       	20 March 2021
 * @brief       Header file for util.c
 * @note        Please refer to the .c file for a detailed description.
 *
 */
// ----------------------------------------------------------------------------

#ifndef UTIL_H_
#define UTIL_H_

bool UTIL_NV_ParamInitCheck_U16(const uint16_t parameter);

uint16_t UTIL_FixMul_U32_U16(const uint32_t fixmul, const uint16_t value);
int16_t UTIL_FixMul_U32_S16(const uint32_t fixmul, const int16_t value);
uint16_t UTIL_FixMulOvf_U32_U16(const uint32_t fixmul, const uint16_t value, bool * const p_overFlow);
int16_t UTIL_FixMulOvf_U32_S16(const uint32_t fixmul, const int16_t value, bool * const p_overflow);
uint32_t UTIL_FixMul_U32_U32(const uint32_t fixmul, const uint32_t value);
int32_t UTIL_FixMul_U32_S32(const uint32_t fixmul, const int32_t value);
uint32_t UTIL_FixMulOvf_U32_U32(const uint32_t fixmul, const uint32_t value, bool * const p_overflow);
int32_t UTIL_FixMulOvf_U32_S32(const uint32_t fixmul, const int32_t value, bool * const p_overflow);
uint16_t UTIL_FixMul_U16_U16(const uint16_t fixmul, const uint8_t value);

bool UTIL_FixMulInverse_U16_U16(const uint16_t realVal, const uint16_t divValue, uint32_t * const result);
uint16_t UTIL_Make_U16(const uint8_t lowByte, const uint8_t highByte);
uint16_t UTIL_FromBytes_U16(const uint8_t * const p_data);
void UTIL_ToBytes_U16(const uint16_t value, uint8_t * const destBuffer);

#endif /* UTIL_H_ */
