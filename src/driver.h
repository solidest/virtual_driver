/*
** driver.h for Virtual Driver in /home/pi/driver/virtual_driver/src
**
** Made by solidest
** Login   <>
**
** Started on  Tue Nov 12 2:23:01 PM 2019 solidest
** Last update Wed Nov 12 4:54:12 PM 2019 solidest
*/

#ifndef DRIVER_H_
# define DRIVER_H_

#include "drvdef.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#define DLL_PUBLIC __attribute((visibility("default")))

DLL_PUBLIC void e_initial(DrvManer* drvManer);
DLL_PUBLIC void e_release();

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* !DRIVER_H_ */
