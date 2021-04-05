/*
 * util.h
 *
 *  Created on: 20.03.21
 *      Author: jsteggall
 */
#ifndef UTIL_H_
#define UTIL_H_

bool UTIL_NV_ParamInitCheck_U16(const uint16_t parameter);
uint16_t UTIL_FixMul_U32_U16(const uint32_t fixmul, const uint16_t value);
int16_t UTIL_FixMul_U32_S16(const uint32_t fixmul, const int16_t value);
bool UTIL_FixMulInverse_U16_U16(const uint16_t realVal, const uint16_t divValue, uint32_t * const result);

#endif /* UTIL_H_ */
