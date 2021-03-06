/**-------------------------------------------------------**
** SCANAPLT.C:	Main entry-points for the Windows NT
**              scanner Control Panel Applet. 
** Environment: Windows NT.
** (C) Hewlett-Packard Company 1993.  PLT.
**-------------------------------------------------------**/

/**------------------ Include Files ---------------------**/
#include <windows.h>
#include <cpl.h>                 /* control panel defines */
#include <winreg.h>                /* registry prototypes */
#include <stdlib.h>                 /* prototype for atoi */
#include "scanaplt.h"

/**-------------- Public Function Prototypes ------------**/
INT APIENTRY LibMain(HANDLE, ULONG, LPVOID);
LONG CALLBACK CPlApplet(HWND, UINT, LPARAM, LPARAM);

/**-------------- Private Function Prototypes -----------**/
BOOL WriteScannerRegistry(HWND, LPTSTR);
BOOL ReadScannerRegistry(HWND, int);
BOOL DeleteScannerRegistry(HWND, LPTSTR);
BOOL ReadInfFile(HWND, LPTSTR);
void ReplaceCommasWithTabs(LPTSTR);
int GetVendorCapabilities(LPTSTR);
void GetVendorModule(LPTSTR, LPTSTR);
void GetVendorValue(LPTSTR, LPTSTR);

/**------------------- Global Data ----------------------**/
HINSTANCE hInst;
int iCap=0, index=0;
char lpBuffer[MAX_STR_LEN+1];

/***********************************************************
** Routine:  LibMain
**    Called by Windows when a DLL is loaded. Perform your
**    process or thread specific initialization tasks here.
** Return:  Return 1 if initialization successful.
***********************************************************/
INT APIENTRY LibMain(HINSTANCE hModule, ULONG ulReason, 
   LPVOID lpReserved)
{
   hInst = hModule;
   return 1;
} /* LibMain */

/***********************************************************
** Routine:  CPlApplet
**    Standard callback entry point for the Control Panel.
***********************************************************/
LONG CALLBACK CPlApplet(HWND hWndCPL, UINT msg, 
   LPARAM lParam1, LPARAM lParam2)
{
   LPNEWCPLINFO lpNewCPlInfo;

   switch (msg)
   {
      case CPL_INIT:
         /* Sent (once) immediately after DLL loaded */
         return TRUE;

      case CPL_GETCOUNT:
         /* Sent after CPL_INIT message, return # of apps */
         return NUM_SCANNER_APPS;

      case CPL_INQUIRE:
         /* obsoleted by CPL_NEWINQUIRE, just return FALSE */
         return FALSE;

      case CPL_NEWINQUIRE:
         /* Sent after CPL_GETCOUNT, once for each applet */
         lpNewCPlInfo = (LPNEWCPLINFO)lParam2;
         lpNewCPlInfo->dwSize = (DWORD)sizeof(NEWCPLINFO);
         lpNewCPlInfo->dwFlags = 0;
         lpNewCPlInfo->dwHelpContext = 0;
         lpNewCPlInfo->lData = 0;
         lpNewCPlInfo->hIcon = LoadIcon(hInst, 
            (LPCTSTR)MAKEINTRESOURCE(ICO_SCANNER));
         lpNewCPlInfo->szHelpFile[0] = '\0';
         strcpy(lpNewCPlInfo->szName, "Scanners");
         strcpy(lpNewCPlInfo->szInfo, 
            "Adds, removes, and configures scanners.");
         break;

      case CPL_SELECT:
         /* Sent when user selects your applet icon */
         break;

      case CPL_DBLCLK:
         /* sent when user double-clicks your applet icon */
         DialogBox(hInst, MAKEINTRESOURCE(SCANNER_DLG), 
            hWndCPL, (DLGPROC)ScannerDlgProc);
         break;

      case CPL_STOP:
         /* Sent once for each app before Cont-Panel ends */
         break;

      case CPL_EXIT:
         /* Sent after last CPL_STOP message */
         break;

      default:  break;
   } /* switch on msg */
   return TRUE;
} /* CPlApplet */

/***********************************************************
** Routine:  ScannerDlgProc
**    Dialog box procedure for SCANNER_DLG
***********************************************************/
BOOL APIENTRY ScannerDlgProc(HWND hDlg, UINT msg, 
   WPARAM wParam, LPARAM lParam)
{
   HINSTANCE hLib = 0;
   FARPROC lpfn = 0;
   static int tabstops[] = { 400, 500, 600 };
   static char lpEntry[MAX_STR_LEN+1];

   switch (msg) 
   {
      case WM_INITDIALOG:
         SendDlgItemMessage(hDlg, ID_SCANNER_LIST, 
            LB_SETTABSTOPS, 3, (long)(int *)tabstops);
         ReadScannerRegistry(hDlg, ID_SCANNER_LIST);
         return TRUE;

      case WM_SYSCOMMAND:
         if (wParam == SC_CLOSE) EndDialog(hDlg, TRUE);
         return FALSE;

      case WM_COMMAND:
         switch(LOWORD(wParam))
         {
            case IDOK:
               EndDialog(hDlg, TRUE);
               return TRUE;

            case ID_SCANNER_LIST:
               if (HIWORD(wParam) != LBN_SELCHANGE) break;
               if ((index = SendMessage((HWND)lParam, 
                  LB_GETCURSEL, 0, 0L)) != -1L)
               {
                  SendMessage((HWND)lParam, LB_GETTEXT, index,
                     (LONG)(LPTSTR)lpBuffer);
                  iCap = GetVendorCapabilities(lpBuffer);
                  if ((iCap & CPL_SCANNER_SUPPORT_CONFIGURE) == 0)
                     EnableWindow(GetDlgItem(hDlg, 
                        ID_SCANNER_CONFIGURE), 0);
                  else EnableWindow(GetDlgItem(hDlg, 
                     ID_SCANNER_CONFIGURE), 1);
               }
               return TRUE;
               
            case ID_SCANNER_HELP:    /* call WinHelp */
               MessageBox(hDlg, (LPTSTR)"You're doing fine!", 
                  (LPTSTR)"Scanners", MB_OK);
               return TRUE;

            case ID_SCANNER_ADD:
               DialogBox(hInst, MAKEINTRESOURCE(ADD_DLG), 
                  hDlg, (DLGPROC)AddDlgProc);
               ReadScannerRegistry(hDlg, ID_SCANNER_LIST);
               break;

            case ID_SCANNER_CONFIGURE:
               SendDlgItemMessage(hDlg, ID_SCANNER_LIST,
                  LB_GETTEXT, index, (LONG)(LPTSTR)lpEntry);
               GetVendorModule(lpEntry, lpBuffer);

               if ((hLib = LoadLibrary(lpBuffer)) == NULL)
               {
                  MessageBox(hDlg, "Couldn't find library!", 
                     "Scanners", MB_OK);
                  break;
               }
               lpfn = GetProcAddress(hLib, "CPL_ScannerConfigure");
               (*lpfn)(hDlg, 0, (LPTSTR)lpEntry);
               FreeLibrary(hLib);
               break;

            case ID_SCANNER_REMOVE:
               SendDlgItemMessage(hDlg, ID_SCANNER_LIST,
                  LB_GETTEXT, index, (LONG)(LPTSTR)lpEntry);
               if (DeleteScannerRegistry(hDlg, lpEntry))
                  ReadScannerRegistry(hDlg, ID_SCANNER_LIST);
               break;

            default:
               return TRUE;
      }
      break;
   }
   return FALSE;
} /* ScannerDlgProc */

/***********************************************************
** Routine:  AddDlgProc - dialog box procedure for ADD_DLG
***********************************************************/
BOOL APIENTRY AddDlgProc(HWND hDlg, UINT msg, WPARAM wParam, 
  LPARAM lParam)
{
   static char lpFilename[MAX_STR_LEN+1];

   switch (msg) 
   {
      case WM_INITDIALOG:
         SendDlgItemMessage(hDlg, ID_EDIT_DRIVESRC,
            EM_LIMITTEXT, MAX_STR_LEN, 0L);
         SetDlgItemText(hDlg, ID_EDIT_DRIVESRC, "A:\\");
         return TRUE;

      case WM_COMMAND:
         switch(LOWORD(wParam))
         {
            case IDOK:
               GetDlgItemText(hDlg, ID_EDIT_DRIVESRC, 
                  lpFilename, MAX_STR_LEN);
               if (lpFilename[strlen(lpFilename)-1] != '\\')
                  strcat(lpFilename, "\\");
               strcat(lpFilename, "SCANAPLT.INF");
               EndDialog(hDlg, ReadInfFile(hDlg, lpFilename));
               return TRUE;

            case ID_ADD_HELP:     /* call WinHelp */
               MessageBox(NULL, (LPTSTR)"You're doing fine!", 
                  (LPSTR)"Scanners", MB_OK);
               return TRUE;

            case IDCANCEL:
               EndDialog(hDlg, FALSE);
               return TRUE;

            default: return TRUE;
         }
         break;
   }
   return FALSE;
} /* AddDlgProc */

/**------------------------------------------------------**/
BOOL ReadScannerRegistry(HWND hDlg, int iListID)
{
   HKEY hKey;
   DWORD i, status;
   CHAR cValueName[MAX_STR_LEN+1], cDataString[MAX_STR_LEN+1];
   DWORD dwValueLen = MAX_STR_LEN, dwDataLen = MAX_STR_LEN;

   SendDlgItemMessage(hDlg, iListID, LB_RESETCONTENT, 0, 0L);

   if ((status = RegOpenKeyEx(HKEY_CURRENT_USER, 
      "Control Panel\\Scanners", 0, KEY_QUERY_VALUE, &hKey)) 
      != ERROR_SUCCESS)
   {
      /* "Scanners" key doesn't exist, create it */
      RegCreateKeyEx(HKEY_CURRENT_USER, 
         "Control Panel\\Scanners", 0, "\0", 
         REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WRITE, 
         NULL, &hKey, &status);
      RegCloseKey(hKey);
      EnableWindow(GetDlgItem(hDlg, ID_SCANNER_REMOVE), 0);
      EnableWindow(GetDlgItem(hDlg, ID_SCANNER_CONFIGURE), 0);
      return FALSE;
   }

   for (i=0, status=ERROR_SUCCESS; status==ERROR_SUCCESS; i++)
   {
      dwValueLen = MAX_STR_LEN;    /* must reset max length */
      dwDataLen = MAX_STR_LEN;     /* must reset max length */
      status = RegEnumValue(hKey, i, cValueName, &dwValueLen,
         NULL, NULL, cDataString, &dwDataLen);
      if (status == ERROR_SUCCESS)
      {
         ReplaceCommasWithTabs(cDataString);
         SendDlgItemMessage(hDlg, iListID, LB_ADDSTRING, 0,
            (long)(LPTSTR)cDataString);
      }
   }
   RegCloseKey(hKey);

   SendDlgItemMessage(hDlg, iListID, LB_SETCURSEL, 0, 0L);
   SendDlgItemMessage(hDlg, iListID, LB_GETTEXT, 0,
      (LONG)(LPTSTR)lpBuffer);
   iCap = GetVendorCapabilities(lpBuffer);
   if ((iCap & CPL_SCANNER_SUPPORT_CONFIGURE) == 0)
      EnableWindow(GetDlgItem(hDlg, ID_SCANNER_CONFIGURE), 0);
   else EnableWindow(GetDlgItem(hDlg, ID_SCANNER_CONFIGURE), 1);
   EnableWindow(GetDlgItem(hDlg, ID_SCANNER_REMOVE), 1);

   return TRUE;
} /* ReadScannerRegistery */

/**------------------------------------------------------**/
BOOL DeleteScannerRegistry(HWND hDlg, LPTSTR lpEntry)
{
   HKEY hKey;
   HINSTANCE hLib = 0;
   FARPROC lpfn = 0;

   if (MessageBox(hDlg, "Remove the selected scanner driver?",
      "Scanners", MB_YESNO | MB_ICONQUESTION) == IDNO) 
       return FALSE;

   if (RegOpenKeyEx(HKEY_CURRENT_USER, 
      "Control Panel\\Scanners", 0, KEY_WRITE, &hKey)
         != ERROR_SUCCESS) return FALSE;
   GetVendorValue(lpEntry, lpBuffer);
   RegDeleteValue(hKey, lpBuffer);
   RegCloseKey(hKey);

   if ((iCap & CPL_SCANNER_SUPPORT_UNINSTALL) != 0)
   {
      GetVendorModule(lpEntry, lpBuffer);
      if ((hLib = LoadLibrary(lpBuffer)) == NULL) return FALSE;
      lpfn = GetProcAddress(hLib, "CPL_ScannerUninstall");
      (*lpfn)(hDlg, 0, (LPTSTR)lpEntry);
      FreeLibrary(hLib);
   }
   return TRUE;
} /* DeleteScannerRegistry */

/**------------------------------------------------------**/
BOOL WriteScannerRegistry(HWND hDlg, LPTSTR lpEntry)
{
   HKEY hKey;
   HINSTANCE hLib = 0;
   FARPROC lpfn = 0;

   if (RegOpenKeyEx(HKEY_CURRENT_USER, 
      "Control Panel\\Scanners", 0, KEY_WRITE, &hKey)
         != ERROR_SUCCESS) return FALSE;
   GetVendorValue(lpEntry, lpBuffer);
   RegSetValueEx(hKey, lpBuffer, 0, (DWORD)REG_SZ,
      (LPBYTE)lpEntry, strlen(lpEntry));
   RegCloseKey(hKey);

   iCap = GetVendorCapabilities(lpEntry);
   if ((iCap & CPL_SCANNER_SUPPORT_INSTALL) != 0)
   {
      GetVendorModule(lpEntry, lpBuffer);
      if ((hLib = LoadLibrary(lpBuffer)) == NULL) return FALSE;
      lpfn = GetProcAddress(hLib, "CPL_ScannerInstall");
      (*lpfn)(hDlg, 0, (LPTSTR)lpEntry);
      FreeLibrary(hLib);
   }
   return TRUE;
} /* WriteScannerRegistry */

/**------------------------------------------------------**/
BOOL ReadInfFile(HWND hDlg, LPTSTR lpFilename)
{
   WORD wDrivers=0, i=0;
   char lpEntry[MAX_STR_LEN+1];

   wDrivers = GetPrivateProfileInt(
      "CPL_Scanner", "Scanners", 0, lpFilename);

   for (i=1; i <= wDrivers; i++)
   {
      wsprintf(lpBuffer, "Scanner%d", i);
      GetPrivateProfileString("CPL_Scanner", lpBuffer, "0", 
         lpEntry, MAX_STR_LEN, lpFilename);
      WriteScannerRegistry(hDlg, lpEntry);
   } /* for i */
   return TRUE;
} /* ReadInfFile */

/**------------------------------------------------------**/
void ReplaceCommasWithTabs(LPTSTR lpEntry)
{
   int i;
   for (i=0; lpEntry[i] != '\0'; i++) 
      if (lpEntry[i] == ',') lpEntry[i] = '\t';
} /* ReplaceCommasWithTabs */

/**------------------------------------------------------**/
int GetVendorCapabilities(LPTSTR lpEntry)
{
   LPTSTR lp = lpEntry + strlen(lpEntry) - 1;
   while (*lp != '\0' && *lp != '\t' && *lp != ',') lp--;
   return atoi(++lp);
} /* GetVendorCapabilities */

/**------------------------------------------------------**/
void GetVendorModule(LPTSTR lpEntry, LPTSTR lpModule)
{
   LPTSTR lp1 = lpEntry, lp2 = lpModule;

   while (*lp1 != '\0' && *lp1 != '\t' && *lp1 != ',') lp1++;
   lp1++;
   while (*lp1 != '\0' && *lp1 != '\t' && *lp1 != ',') 
      *lp2++ = *lp1++;
   *lp2 = '\0';
} /* GetVendorModule */

/**------------------------------------------------------**/
void GetVendorValue(LPTSTR lpEntry, LPTSTR lpValue)
{
   LPTSTR lp1 = lpEntry, lp2 = lpValue;

   while (*lp1 != '\0' && *lp1 != '\t' && *lp1 != ',') lp1++;
   lp1++;
   while (*lp1 != '\0' && *lp1 != '\t' && *lp1 != ',') lp1++;
   lp1++;
   while (*lp1 != '\0' && *lp1 != '\t' && *lp1 != ',') 
      *lp2++ = *lp1++;
   *lp2 = '\0';
} /* GetVendorValue */
