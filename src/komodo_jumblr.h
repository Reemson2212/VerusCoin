/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

/*
 z_exportkey "zaddr"
 z_exportwallet "filename"
 z_getoperationstatus (["operationid", ... ])
 z_gettotalbalance ( minconf )
 z_importkey "zkey" ( rescan )
 z_importwallet "filename"
 z_listaddresses
 z_sendmany "fromaddress" [{"address":... ,"amount":..., "memo":"<hex>"},...] ( minconf ) ( fee )
 */

#define JUMBLR_ADDR "RGhxXpXSSBTBm9EvNsXnTQczthMCxHX91t"
#define JUMBLR_BTCADDR "18RmTJe9qMech8siuhYfMtHo8RtcN1obC6"
#define JUMBLR_MAXSECRETADDRS 777
#define JUMBLR_SYNCHRONIZED_BLOCKS 2 // 60
#define JUMBLR_INCR (99.65 / 100)
#define JUMBLR_FEE 0.001
#define JUMBLR_TXFEE 0.01
#define SMALLVAL 0.000000000000001

#define JUMBLR_ERROR_DUPLICATEDEPOSIT -1
#define JUMBLR_ERROR_SECRETCANTBEDEPOSIT -2
#define JUMBLR_ERROR_TOOMANYSECRETS -3
#define JUMBLR_ERROR_IMPORTADDRESS -4

struct jumblr_item
{
    UT_hash_handle hh;
    int64_t amount,fee,txfee;
    uint32_t spent,pad;
    char opid[64],src[128],dest[128],status;
} *Jumblrs;

char Jumblr_secretaddrs[JUMBLR_MAXSECRETADDRS][64],Jumblr_deposit[64];
int32_t Jumblr_numsecretaddrs; // if 0 -> run silent mode

char *jumblr_issuemethod(char *userpass,char *method,char *params,uint16_t port)
{
    cJSON *retjson,*resjson = 0; char *retstr;
    if ( (retstr= komodo_issuemethod(userpass,method,params,port)) != 0 )
    {
        if ( (retjson= cJSON_Parse(retstr)) != 0 )
        {
            if ( jobj(retjson,(char *)"result") != 0 )
                resjson = jduplicate(jobj(retjson,(char *)"result"));
            else if ( jobj(retjson,(char *)"error") != 0 )
                resjson = jduplicate(jobj(retjson,(char *)"error"));
            else
            {
                resjson = cJSON_CreateObject();
                jaddstr(resjson,(char *)"error",(char *)"cant parse return");
            }
            free_json(retjson);
        }
        free(retstr);
    }
    if ( resjson != 0 )
        return(jprint(resjson,1));
    else return(clonestr((char *)"{\"error\":\"unknown error\"}"));
}

char *jumblr_importaddress(char *address)
{
    char params[1024];
    sprintf(params,"[\"%s\", \"%s\", false]",address,address);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"importaddress",params,7771));
}

int32_t Jumblr_secretaddrfind(char *searchaddr)
{
    int32_t i;
    for (i=0; i<Jumblr_numsecretaddrs; i++)
    {
        if ( strcmp(searchaddr,Jumblr_secretaddrs[i]) == 0 )
            return(i);
    }
    return(-1);
}

int32_t Jumblr_secretaddradd(char *secretaddr) // external
{
    int32_t ind;
    if ( secretaddr != 0 && secretaddr[0] != 0 )
    {
        if ( Jumblr_numsecretaddrs < JUMBLR_MAXSECRETADDRS )
        {
            if ( strcmp(Jumblr_deposit,secretaddr) != 0 )
            {
                if ( (ind= Jumblr_secretaddrfind(secretaddr)) < 0 )
                {
                    safecopy(Jumblr_secretaddrs[Jumblr_numsecretaddrs],secretaddr,64);
                    Jumblr_numsecretaddrs++;
                } else return(ind);
            } else return(JUMBLR_ERROR_SECRETCANTBEDEPOSIT);
        } else return(JUMBLR_ERROR_TOOMANYSECRETS);
    }
    else
    {
        memset(Jumblr_secretaddrs,0,sizeof(Jumblr_secretaddrs));
        Jumblr_numsecretaddrs = 0;
    }
    return(Jumblr_numsecretaddrs);
}

int32_t Jumblr_depositaddradd(char *depositaddr) // external
{
    int32_t ind,retval = JUMBLR_ERROR_DUPLICATEDEPOSIT; char *retstr; cJSON *retjson;
    if ( depositaddr == 0 )
        depositaddr = (char *)"";
    if ( (ind= Jumblr_secretaddrfind(depositaddr)) < 0 )
    {
        safecopy(Jumblr_deposit,depositaddr,sizeof(Jumblr_deposit));
        if ( (retstr= jumblr_importaddress(depositaddr)) != 0 )
        {
            if ( (retjson= cJSON_Parse(retstr)) != 0 )
            {
                if ( jobj(retjson,(char *)"error") == 0 || is_cJSON_Null(jobj(retjson,(char *)"error")) != 0 )
                    retval = 0;
                else retval = JUMBLR_ERROR_IMPORTADDRESS;
                free_json(retjson);
            }
            free(retstr);
        }
    }
    return(retval);
}

int32_t Jumblr_secretaddr(char *secretaddr)
{
    uint32_t r;
    if ( Jumblr_numsecretaddrs > 0 )
    {
        OS_randombytes((uint8_t *)&r,sizeof(r));
        r %= Jumblr_numsecretaddrs;
        safecopy(secretaddr,Jumblr_secretaddrs[r],64);
    }
    return(r);
}

int32_t jumblr_addresstype(char *addr)
{
    if ( addr[0] == 'z' && addr[1] == 'c' && strlen(addr) >= 40 )
        return('z');
    else if ( strlen(addr) < 40 )
        return('t');
    else return(-1);
}

struct jumblr_item *jumblr_opidfind(char *opid)
{
    struct jumblr_item *ptr;
    HASH_FIND(hh,Jumblrs,opid,(int32_t)strlen(opid),ptr);
    return(ptr);
}

struct jumblr_item *jumblr_opidadd(char *opid)
{
    struct jumblr_item *ptr = 0;
    if ( opid != 0 && (ptr= jumblr_opidfind(opid)) == 0 )
    {
        ptr = (struct jumblr_item *)calloc(1,sizeof(*ptr));
        safecopy(ptr->opid,opid,sizeof(ptr->opid));
        HASH_ADD_KEYPTR(hh,Jumblrs,ptr->opid,(int32_t)strlen(ptr->opid),ptr);
        if ( ptr != jumblr_opidfind(opid) )
            printf("jumblr_opidadd.(%s) ERROR, couldnt find after add\n",opid);
    }
    return(ptr);
}

char *jumblr_validateaddress(char *addr)
{
    char params[1024];
    sprintf(params,"[\"%s\"]",addr);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"validateaddress",params,7771));
}

char *jumblr_zgetnewaddress()
{
    char params[1024];
    sprintf(params,"[]");
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"z_getnewaddress",params,7771));
}

char *jumblr_zlistoperationids()
{
    char params[1024];
    sprintf(params,"[]");
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"z_listoperationids",params,7771));
}

char *jumblr_zgetoperationresult(char *opid)
{
    char params[1024];
    sprintf(params,"[[\"%s\"]]",opid);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"z_getoperationresult",params,7771));
}

char *jumblr_zgetoperationstatus(char *opid)
{
    char params[1024];
    sprintf(params,"[[\"%s\"]]",opid);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"z_getoperationstatus",params,7771));
}

char *jumblr_sendt_to_z(char *taddr,char *zaddr,double amount)
{
    char params[1024]; double fee = (amount-3*JUMBLR_TXFEE) * JUMBLR_FEE;
    if ( jumblr_addresstype(zaddr) != 'z' || jumblr_addresstype(taddr) != 't' )
        return(clonestr((char *)"{\"error\":\"illegal address in t to z\"}"));
    sprintf(params,"[\"%s\", [{\"address\":\"%s\",\"amount\":%.8f}, {\"address\":\"%s\",\"amount\":%.8f}], 1, %.8f]",taddr,zaddr,amount-fee-JUMBLR_TXFEE,JUMBLR_ADDR,fee,JUMBLR_TXFEE);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"z_sendmany",params,7771));
}

char *jumblr_sendz_to_z(char *zaddrS,char *zaddrD,double amount)
{
    char params[1024]; double fee = (amount-2*JUMBLR_TXFEE) * JUMBLR_FEE;
    if ( jumblr_addresstype(zaddrS) != 'z' || jumblr_addresstype(zaddrD) != 'z' )
        return(clonestr((char *)"{\"error\":\"illegal address in z to z\"}"));
    sprintf(params,"[\"%s\", [{\"address\":\"%s\",\"amount\":%.8f}, {\"address\":\"%s\",\"amount\":%.8f}], 1, %.8f]",zaddrS,zaddrD,amount-fee-JUMBLR_TXFEE,JUMBLR_ADDR,fee,JUMBLR_TXFEE);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"z_sendmany",params,7771));
}

char *jumblr_sendz_to_t(char *zaddr,char *taddr,double amount)
{
    char params[1024]; double fee = (amount-JUMBLR_TXFEE) * JUMBLR_FEE;
    if ( jumblr_addresstype(zaddr) != 'z' || jumblr_addresstype(taddr) != 't' )
        return(clonestr((char *)"{\"error\":\"illegal address in z to t\"}"));
    sprintf(params,"[\"%s\", [{\"address\":\"%s\",\"amount\":%.8f}, {\"address\":\"%s\",\"amount\":%.8f}], 1, %.8f]",zaddr,taddr,amount-fee-JUMBLR_TXFEE,JUMBLR_ADDR,fee,JUMBLR_TXFEE);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"z_sendmany",params,7771));
}

char *jumblr_zlistreceivedbyaddress(char *addr)
{
    char params[1024];
    sprintf(params,"[\"%s\", 1]",addr);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"z_listreceivedbyaddress",params,7771));
}

char *jumblr_getreceivedbyaddress(char *addr)
{
    char params[1024];
    sprintf(params,"[\"%s\", 1]",addr);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"getreceivedbyaddress",params,7771));
}

char *jumblr_importprivkey(char *wifstr)
{
    char params[1024];
    sprintf(params,"[\"%s\", \"\", false]",wifstr);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"importprivkey",params,7771));
}

char *jumblr_zgetbalance(char *addr)
{
    char params[1024];
    sprintf(params,"[\"%s\", 1]",addr);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"z_getbalance",params,7771));
}

char *jumblr_listunspent(char *coinaddr)
{
    char params[1024];
    sprintf(params,"[1, 99999999, [\"%s\"]]",coinaddr);
    return(jumblr_issuemethod(KMDUSERPASS,(char *)"listunspent",params,7771));
}

int64_t jumblr_receivedby(char *addr)
{
    char *retstr; int64_t total = 0;
    if ( (retstr= jumblr_getreceivedbyaddress(addr)) != 0 )
    {
        total = atof(retstr) * SATOSHIDEN;
        free(retstr);
    }
    return(total);
}

int64_t jumblr_balance(char *addr)
{
    char *retstr; double val; cJSON *retjson; int32_t i,n; int64_t balance = 0;
    if ( jumblr_addresstype(addr) == 't' )
    {
        if ( (retstr= jumblr_listunspent(addr)) != 0 )
        {
            printf("jumblr.[%s].(%s)\n","KMD",retstr);
            if ( (retjson= cJSON_Parse(retstr)) != 0 )
            {
                if ( (n= cJSON_GetArraySize(retjson)) > 0 && is_cJSON_Array(retjson) != 0 )
                    for (i=0; i<n; i++)
                        balance += SATOSHIDEN * jdouble(jitem(retjson,i),(char *)"amount");
                free_json(retjson);
            }
            free(retstr);
        }
    }
    else if ( (retstr= jumblr_zgetbalance(addr)) != 0 )
    {
        if ( (val= atof(retstr)) > SMALLVAL )
            balance = val * SATOSHIDEN;
        free(retstr);
    }
    return(balance);
}

int32_t jumblr_itemset(struct jumblr_item *ptr,cJSON *item,char *status)
{
    cJSON *params,*amounts,*dest; char *from,*addr; int32_t i,n; int64_t amount;
    /*"params" : {
     "fromaddress" : "RDhEGYScNQYetCyG75Kf8Fg61UWPdwc1C5",
     "amounts" : [
     {
     "address" : "zc9s3UdkDFTnnwHrMCr1vYy2WmkjhmTxXNiqC42s7BjeKBVUwk766TTSsrRPKfnX31Bbu8wbrTqnjDqskYGwx48FZMPHvft",
     "amount" : 3.00000000
     }
     ],
     "minconf" : 1,
     "fee" : 0.00010000
     }*/
    if ( (params= jobj(item,(char *)"params")) != 0 )
    {
        //printf("params.(%s)\n",jprint(params,0));
        if ( (from= jstr(params,(char *)"fromaddress")) != 0 )
        {
            safecopy(ptr->src,from,sizeof(ptr->src));
        }
        if ( (amounts= jarray(&n,params,(char *)"amounts")) != 0 )
        {
            for (i=0; i<n; i++)
            {
                dest = jitem(amounts,i);
                //printf("%s ",jprint(dest,0));
                if ( (addr= jstr(dest,(char *)"address")) != 0 && (amount= jdouble(dest,(char *)"amount")*SATOSHIDEN) > 0 )
                {
                    if ( strcmp(addr,JUMBLR_ADDR) == 0 )
                        ptr->fee = amount;
                    else
                    {
                        ptr->amount = amount;
                        safecopy(ptr->dest,addr,sizeof(ptr->dest));
                    }
                }
            }
        }
        ptr->txfee = jdouble(params,(char *)"fee") * SATOSHIDEN;
    }
    return(1);
}

void jumblr_opidupdate(struct jumblr_item *ptr)
{
    char *retstr,*status; cJSON *retjson,*item;
    if ( ptr->status == 0 )
    {
        if ( (retstr= jumblr_zgetoperationstatus(ptr->opid)) != 0 )
        {
            if ( (retjson= cJSON_Parse(retstr)) != 0 )
            {
                if ( cJSON_GetArraySize(retjson) == 1 && is_cJSON_Array(retjson) != 0 )
                {
                    item = jitem(retjson,0);
                    //printf("%s\n",jprint(item,0));
                    if ( (status= jstr(item,(char *)"status")) != 0 )
                    {
                        if ( strcmp(status,(char *)"success") == 0 )
                        {
                            ptr->status = jumblr_itemset(ptr,item,status);
                            if ( (jumblr_addresstype(ptr->src) == 't' && jumblr_addresstype(ptr->src) == 'z' && strcmp(ptr->src,Jumblr_deposit) != 0) || (jumblr_addresstype(ptr->src) == 'z' && jumblr_addresstype(ptr->src) == 't' && Jumblr_secretaddrfind(ptr->dest) < 0) )
                            {
                                printf("a non-jumblr t->z pruned\n");
                                free(jumblr_zgetoperationresult(ptr->opid));
                                ptr->status = -1;
                            }

                        }
                        else if ( strcmp(status,(char *)"failed") == 0 )
                        {
                            printf("jumblr_opidupdate %s failed\n",ptr->opid);
                            free(jumblr_zgetoperationresult(ptr->opid));
                            ptr->status = -1;
                        }
                    }
                }
                free_json(retjson);
            }
            free(retstr);
        }
    }
}

void jumblr_prune(struct jumblr_item *ptr)
{
    struct jumblr_item *tmp; char oldsrc[128]; int32_t flag = 1;
    printf("jumblr_prune %s\n",ptr->opid);
    strcpy(oldsrc,ptr->src);
    free(jumblr_zgetoperationresult(ptr->opid));
    while ( flag != 0 )
    {
        flag = 0;
        HASH_ITER(hh,Jumblrs,ptr,tmp)
        {
            if ( strcmp(oldsrc,ptr->dest) == 0 )
            {
                printf("jumblr_prune %s (%s -> %s) matched oldsrc\n",ptr->opid,ptr->src,ptr->dest);
                free(jumblr_zgetoperationresult(ptr->opid));
                strcpy(oldsrc,ptr->src);
                flag = 1;
                break;
            }
        }
    }
}

void jumblr_opidsupdate()
{
    char *retstr; cJSON *array; int32_t i,n; struct jumblr_item *ptr;
    if ( (retstr= jumblr_zlistoperationids()) != 0 )
    {
        if ( (array= cJSON_Parse(retstr)) != 0 )
        {
            if ( (n= cJSON_GetArraySize(array)) > 0 && is_cJSON_Array(array) != 0 )
            {
                printf("%s -> n%d\n",retstr,n);
                for (i=0; i<n; i++)
                {
                    if ( (ptr= jumblr_opidadd(jstri(array,i))) != 0 )
                    {
                        if ( ptr->status == 0 )
                            jumblr_opidupdate(ptr);
                        //printf("%d: %s -> %s %.8f\n",ptr->status,ptr->src,ptr->dest,dstr(ptr->amount));
                        if ( jumblr_addresstype(ptr->src) == 'z' && jumblr_addresstype(ptr->dest) == 't' )
                            jumblr_prune(ptr);
                    }
                }
            }
            free_json(array);
        }
        free(retstr);
    }
}

void jumblr_iteration()
{
    static int32_t lastheight;
    char *zaddr,*retstr,secretaddr[64]; int32_t iter,height,counter,chosen_one,n; uint64_t amount=0,total=0; double fee; struct jumblr_item *ptr,*tmp; uint8_t r,s;
    height = (int32_t)chainActive.Tip()->nHeight;
    if ( lastheight == height )
        return;
    if ( (height % JUMBLR_SYNCHRONIZED_BLOCKS) != 0 )
        return;
    fee = JUMBLR_INCR * JUMBLR_FEE;
    OS_randombytes(&r,sizeof(r));
    s = ((r >> 2) % 3);
    switch ( s )
    {
        case 0: // public -> z, need to importprivkey
            if ( Jumblr_deposit[0] != 0 && (total= jumblr_balance(Jumblr_deposit)) >= (JUMBLR_INCR + 3*(fee+JUMBLR_TXFEE))*SATOSHIDEN )
            {
                if ( (zaddr= jumblr_zgetnewaddress()) != 0 )
                {
                    amount = 0;
                    if ( (height % (JUMBLR_SYNCHRONIZED_BLOCKS*JUMBLR_SYNCHRONIZED_BLOCKS)) == 0 && total >= SATOSHIDEN * ((JUMBLR_INCR + 3*fee)*100 + 3*JUMBLR_TXFEE) )
                        amount = SATOSHIDEN * ((JUMBLR_INCR + 3*fee)*100 + 3*JUMBLR_TXFEE);
                    else if ( (r & 3) == 0 && total >= SATOSHIDEN * ((JUMBLR_INCR + 3*fee)*10 + 3*JUMBLR_TXFEE) )
                        amount = SATOSHIDEN * ((JUMBLR_INCR + 3*fee)*10 + 3*JUMBLR_TXFEE);
                    else amount = SATOSHIDEN * ((JUMBLR_INCR + 3*fee) + 3*JUMBLR_TXFEE);
                    if ( amount > 0 && (retstr= jumblr_sendt_to_z(Jumblr_deposit,zaddr,dstr(amount))) != 0 )
                    {
                        printf("sendt_to_z.(%s)\n",retstr);
                        free(retstr);
                    }
                    free(zaddr);
                } else printf("no zaddr from jumblr_zgetnewaddress\n");
            }
            else if ( Jumblr_deposit[0] != 0 )
                printf("%s total %.8f vs %.8f\n",Jumblr_deposit,dstr(total),(JUMBLR_INCR + 3*(fee+JUMBLR_TXFEE)));
            break;
        case 1: // z -> z
            jumblr_opidsupdate();
            chosen_one = -1;
            for (iter=counter=0; iter<2; iter++)
            {
                counter = n = 0;
                HASH_ITER(hh,Jumblrs,ptr,tmp)
                {
                    if ( jumblr_addresstype(ptr->src) == 't' && jumblr_addresstype(ptr->dest) == 'z' )
                    {
                        if ( ptr->spent == 0 && (total= jumblr_balance(ptr->dest)) >= (fee + JUMBLR_FEE)*SATOSHIDEN )
                        {
                            if ( iter == 1 && counter == chosen_one )
                            {
                                if ( (zaddr= jumblr_zgetnewaddress()) != 0 )
                                {
                                    if ( (retstr= jumblr_sendz_to_z(ptr->dest,zaddr,dstr(total))) != 0 )
                                    {
                                        printf("n.%d counter.%d chosen_one.%d sendz_to_z.(%s)\n",n,counter,chosen_one,retstr);
                                        free(retstr);
                                    }
                                    ptr->spent = (uint32_t)time(NULL);
                                    free(zaddr);
                                    break;
                                }
                            }
                            counter++;
                        }
                    }
                    n++;
                }
                if ( counter == 0 )
                    break;
                if ( iter == 0 )
                {
                    OS_randombytes((uint8_t *)&chosen_one,sizeof(chosen_one));
                    if ( chosen_one < 0 )
                        chosen_one = -chosen_one;
                    chosen_one %= counter;
                    printf("jumblr z->z chosen_one.%d of %d, from %d\n",chosen_one,counter,n);
                }
            }
            break;
        case 2: // z -> public
            if ( Jumblr_numsecretaddrs > 0 )
            {
                jumblr_opidsupdate();
                chosen_one = -1;
                for (iter=0; iter<2; iter++)
                {
                    counter = n = 0;
                    HASH_ITER(hh,Jumblrs,ptr,tmp)
                    {
                        if ( jumblr_addresstype(ptr->src) == 'z' && jumblr_addresstype(ptr->dest) == 'z' )
                        {
                            if ( ptr->spent == 0 && (total= jumblr_balance(ptr->dest)) >= (fee + JUMBLR_FEE)*SATOSHIDEN )
                            {
                                if ( iter == 1 && n == chosen_one )
                                {
                                    Jumblr_secretaddr(secretaddr);
                                    if ( (retstr= jumblr_sendz_to_t(ptr->dest,secretaddr,dstr(total))) != 0 )
                                    {
                                        printf("sendz_to_t.(%s)\n",retstr);
                                        free(retstr);
                                    }
                                    ptr->spent = (uint32_t)time(NULL);
                                    break;
                                }
                                counter++;
                            }
                        }
                        n++;
                    }
                    if ( counter == 0 )
                        break;
                    if ( iter == 0 )
                    {
                        OS_randombytes((uint8_t *)&chosen_one,sizeof(chosen_one));
                        if ( chosen_one < 0 )
                            chosen_one = -chosen_one;
                        chosen_one %= counter;
                        printf("jumblr z->t chosen_one.%d of %d, from %d\n",chosen_one,counter,n);
                    }
                }
            }
            break;
    }
}
