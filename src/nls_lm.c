#include <R.h>
#include <Rdefines.h>
#include "minpack_lm.h"


int niter;
OptStruct OS;

SEXP nls_lm(SEXP par_arg, SEXP fn, SEXP jac, SEXP control, SEXP rho)
{
    int     i, j;
    int     n, m, ldfjac;
    int     info, nfev, njev;

    double  *par, *fvec, *fjac, *qtf, *wa1, *wa2, *wa3, *wa4,
      *perm, *perm_t, *r, *r2, *r2_x_perm_t, *hess;
    int     *ipvt;

    SEXP    eval_test;
    SEXP    sexp_diag, sexp_hess, sexp_fvec, sexp_info, sexp_niter, 
      sexp_message;
    SEXP    out, out_names;

    char    lmfun_name[8], message[256];

    int     maxfev, maxiter, nprint;
    int     mode;
    
    PROTECT_INDEX ipx;

    OS = (OptStruct) R_alloc(1, sizeof(opt_struct));

    PROTECT(OS->par = duplicate(par_arg));
    n = length(OS->par);

    switch (TYPEOF(OS->par)) {
    case REALSXP:
        break;
    case VECSXP:
        for (i = 0; i < n; i++)
            SET_VECTOR_ELT(OS->par, i, AS_NUMERIC(VECTOR_ELT(OS->par, i)));
        break;
    default:
        error("`par' that you provided is non-list and non-numeric!");
    }

    if (!isFunction(fn)) error("fn is not a function!");
    PROTECT(OS->fcall = lang2(fn, OS->par));

    if (!isEnvironment(rho)) error("rho is not an environment!");
    OS->env = rho;

    PROTECT(eval_test = eval(OS->fcall, OS->env));
    if (!IS_NUMERIC(eval_test))
        error("evaluation of fn function returns non-numeric vector!");
    m = length(eval_test);
    UNPROTECT(1);

    ldfjac = m;

    par         = real_vector(n);
    fvec        = real_vector(m);
    fjac        = real_vector(ldfjac * n);
    qtf         = real_vector(n);
    wa1         = real_vector(n);
    wa2         = real_vector(n);
    wa3         = real_vector(n);
    wa4         = real_vector(m);
    ipvt        =  int_vector(n);
    perm        = real_vector(n * n);
    perm_t      = real_vector(n * n);
    r           = real_vector(n * n);
    r2          = real_vector(n * n);
    r2_x_perm_t = real_vector(n * n);
    hess        = real_vector(n * n);

    OS->ftol    = NUMERIC_VALUE(getListElement(control, "ftol"));
    OS->ptol    = NUMERIC_VALUE(getListElement(control, "ptol"));
    OS->gtol    = NUMERIC_VALUE(getListElement(control, "gtol"));
    OS->epsfcn  = NUMERIC_VALUE(getListElement(control, "epsfcn"));
    OS->factor  = NUMERIC_VALUE(getListElement(control, "factor"));
    OS->diag    = real_vector(n);

    PROTECT_WITH_INDEX(sexp_diag = getListElement(control, "diag"), &ipx);
    switch (TYPEOF(sexp_diag)) {
    case REALSXP:
        if (length(sexp_diag) == n) {
            REPROTECT(sexp_diag = duplicate(sexp_diag), ipx);
            for (i = 0; i < n; i++)
                OS->diag[i] = NUMERIC_POINTER(sexp_diag)[i];
            mode = 2;
        }
        else {
            REPROTECT(sexp_diag = NEW_NUMERIC(n), ipx);
            mode = 1;
        }
        break;
    case VECSXP:
        if (length(sexp_diag) == n) {
            REPROTECT(sexp_diag = duplicate(sexp_diag), ipx);
            for (i = 0; i < n; i++) {
                SET_VECTOR_ELT(sexp_diag, i, AS_NUMERIC(VECTOR_ELT(sexp_diag, i)));
                OS->diag[i] = NUMERIC_VALUE(VECTOR_ELT(sexp_diag, i));
            }
            mode = 2;
        }
        else {
            REPROTECT(sexp_diag = NEW_LIST(n), ipx);
            for (i = 0; i < n; i++)
                SET_VECTOR_ELT(sexp_diag, i, NEW_NUMERIC(1));
            mode = 1;
        }
        break;
    default:
        error("`diag' that you provided is non-list and non-numeric!");
    }
    maxfev = INTEGER_VALUE(getListElement(control, "maxfev"));
    maxiter = INTEGER_VALUE(getListElement(control, "maxiter"));
    nprint = INTEGER_VALUE(getListElement(control, "nprint"));

    if (IS_NUMERIC(OS->par)) {
        for (i = 0; i < n; i++)
            par[i] = NUMERIC_POINTER(OS->par)[i];
    }
    else {
        for (i = 0; i < n; i++)
            par[i] = NUMERIC_VALUE(VECTOR_ELT(OS->par, i));
    }

    niter = 0;

/*========================================================================*/

    if (isNull(jac)) {
      F77_CALL(lmdif)(&fcn_lmdif, &m, &n, par, fvec,
		      &OS->ftol, &OS->ptol, &OS->gtol,
		      &maxfev, &maxiter, &OS->epsfcn, OS->diag, &mode,
		      &OS->factor, &nprint, &info, &nfev, fjac, &ldfjac,
		      ipvt, qtf, wa1, wa2, wa3, wa4);
      strcpy(lmfun_name, "lmdif");
    }
    else {
      if (!isFunction(jac))
	error("jac is not a function!");
      PROTECT(OS->jcall = lang2(jac, OS->par));
      PROTECT(eval_test = eval(OS->jcall, OS->env));
      if (!IS_NUMERIC(eval_test))
	error("evaluation of jac function returns non-numeric vector!");
      if (length(eval_test) != n*m)
	error("jac function must return numeric vector with length"
	      " == length(par) * length(fn(par, ...)). Your function"
	      " returns one with length %d while %d expected.",
	      length(eval_test), n*m);
        UNPROTECT(1);
        F77_CALL(lmder)(&fcn_lmder, &m, &n, par, fvec,
			fjac, &ldfjac,
                        &OS->ftol, &OS->ptol, &OS->gtol,
                        &maxfev, &maxiter, OS->diag, &mode,
                        &OS->factor, &nprint, &info, &nfev, &njev,
			ipvt, qtf, wa1, wa2, wa3, wa4);
        UNPROTECT(1);
        strcpy(lmfun_name, "lmder");
    }
    
/*========================================================================*/
    
    fcn_message(message, info, maxfev, maxiter);
    if (info < 1 || 9 < info)
      error("%s: info = %d. %s\n\n", lmfun_name, info, message);
    
    PROTECT(sexp_hess = NEW_NUMERIC(n*n));
    for (j = 0; j < n; j++)
        for (i = 0; i < n; i++) {
            perm[j*n + i] = (i + 1 == ipvt[j]) ? 1 : 0;
            r   [j*n + i] = (i <= j) ? fjac[j*ldfjac + i] : 0;
        }

    /*  perm %*% t(r) %*% r %*% t(perm)  *
     *    |       |___r2__|         |    *
     *    |           |_r2_x_perm_t_|    *
     *    |_______hess_______|           */

    transpose(perm, n, n, perm_t);
    crossprod(r,    n, n, r,           n, n, r2);
    matprod  (r2,   n, n, perm_t,      n, n, r2_x_perm_t);
    matprod  (perm, n, n, r2_x_perm_t, n, n, hess);

    for (i = 0; i < n*n; i++)
        NUMERIC_POINTER(sexp_hess)[i] = hess[i];

    PROTECT(sexp_fvec = NEW_NUMERIC(m));
    for (i = 0; i < m; i++)
        NUMERIC_POINTER(sexp_fvec)[i] = fvec[i];

    PROTECT(sexp_info = NEW_INTEGER(1));
    INTEGER_POINTER(sexp_info)[0] = info;
    
    PROTECT(sexp_niter = NEW_INTEGER(1));
    INTEGER_POINTER(sexp_niter)[0] = niter;

    PROTECT(sexp_message = NEW_STRING(1));
    SET_STRING_ELT(sexp_message, 0, mkChar(message));
    if (IS_NUMERIC(sexp_diag)) {
        for (i = 0; i < n; i++)
            NUMERIC_POINTER(sexp_diag)[i] = OS->diag[i];
    }
    else {
      for (i = 0; i < n; i++)
	NUMERIC_POINTER(VECTOR_ELT(sexp_diag, i))[0] = OS->diag[i];
    }
    
    PROTECT(out = NEW_LIST(7));
    SET_VECTOR_ELT(out, 0, OS->par);
    SET_VECTOR_ELT(out, 1, sexp_hess);
    SET_VECTOR_ELT(out, 2, sexp_fvec);
    SET_VECTOR_ELT(out, 3, sexp_info);
    SET_VECTOR_ELT(out, 4, sexp_message);
    SET_VECTOR_ELT(out, 5, sexp_diag);
    SET_VECTOR_ELT(out, 6, sexp_niter);
        
    PROTECT(out_names = NEW_STRING(7));
    SET_STRING_ELT(out_names, 0, mkChar("par"));
    SET_STRING_ELT(out_names, 1, mkChar("hessian"));
    SET_STRING_ELT(out_names, 2, mkChar("fvec"));
    SET_STRING_ELT(out_names, 3, mkChar("info"));
    SET_STRING_ELT(out_names, 4, mkChar("message"));
    SET_STRING_ELT(out_names, 5, mkChar("diag"));
    SET_STRING_ELT(out_names, 6, mkChar("niter"));
        
    SET_NAMES(out, out_names);

    UNPROTECT(10);

    return out;
}