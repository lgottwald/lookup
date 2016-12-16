/* QueryLibrary for lookuplib */
#define NoFunc 1
LKP_API int LKP_CALLCONV QueryLibrary(const int funcnr, const int query, int *iv, char **pv)
{
  struct qdrec {
    char *FuncName,
         *FuncExp;
    int  FuncData[5],
         EndoData[10];
    char *ArgSData[10];
  } qd[] = {
    (char*) "Lookup", (char*) "Lookup.1.0.2.2.1.val.E",0,1,0,2,2,1,0,0,0,0,0,0,0,0,0,(char*) "val",(char*) "VARNAME",(char*) "",(char*) "",(char*) "",(char*) "",(char*) "",(char*) "",(char*) "",(char*) ""
           };

  if(funcnr<=0) {
    *iv = -1;
    *pv = (char*)"Bad parameter(s)";
    switch(query) {
      case 0:
        *iv = APIVER;
      break;
      case 1:
        *iv = LIBVER;
        *pv = (char*) "GAMS Development Corporation";
      break;
      case 2:
        *iv = NoFunc;
        *pv = (char*) "Lookups from mdl files";
      break;
    }
  }

  if(funcnr>=1 && funcnr<=NoFunc) {
    switch(query) {
      case 1:
        *pv = qd[funcnr-1].FuncName;
        *iv = 0;
      break;
      case 2:
        *pv = qd[funcnr-1].FuncExp;
        *iv = 0;
      break;
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
        *pv = NULL;
        *iv = qd[funcnr-1].FuncData[query-3];
      break;
      case 1001:
      case 1002:
      case 1003:
      case 1004:
      case 1005:
      case 1006:
      case 1007:
      case 1008:
      case 1009:
      case 1010:
        if(query-1001<=qd[funcnr-1].FuncData[4]) {    /* check against maxargs */
          *iv = qd[funcnr-1].EndoData[query - 1001];
          *pv = (char*) qd[funcnr-1].ArgSData[query - 1001];
        } else
          return 0;
        break;
    }
  }
  return 1;
}
