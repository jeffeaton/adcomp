// Copyright (C) 2013-2015 Kasper Kristensen
// License: GPL-2

/* 
   ================================================
   Routines depending on the openmp runtime library
   ================================================ 
*/
#ifdef _OPENMP
#ifdef WITH_LIBTMB
bool in_parallel();
size_t thread_num();
void start_parallel();
#else
bool in_parallel(){
  return static_cast<bool>(omp_in_parallel());
}
size_t thread_num(){
  return static_cast<size_t>(omp_get_thread_num());
}
void start_parallel(){
#ifdef CPPAD_FRAMEWORK
  CppAD::thread_alloc::free_all();
#endif // CPPAD_FRAMEWORK
  int nthreads=omp_get_max_threads();
  if(config.trace.parallel)
    std::cout << "Using " << nthreads <<  " threads\n";
#ifdef CPPAD_FRAMEWORK
  CppAD::thread_alloc::parallel_setup(nthreads,in_parallel,thread_num);
  CppAD::parallel_ad<AD<AD<AD<double> > > >();
  CppAD::parallel_ad<AD<AD<double> > >();
  CppAD::parallel_ad<AD<double> >();
  CppAD::parallel_ad<double >();
#endif // CPPAD_FRAMEWORK
}
#endif
#endif


/* 
   ================================================
   Templates to do parallel computations
   ================================================
*/
template<class ADFunType>
struct sphess_t{
  sphess_t(ADFunType* pf_,vector<int> i_,vector<int> j_){pf=pf_;i=i_;j=j_;}
  ADFunType* pf;
  vector<int> i;
  vector<int> j;
};

#ifdef CPPAD_FRAMEWORK
#define ADFUN ADFun<double>
#endif // CPPAD_FRAMEWORK
#ifdef TMBAD_FRAMEWORK
#define ADFUN TMBad::ADFun<TMBad::ad_aug>
#endif // TMBAD_FRAMEWORK

/** \brief sphess_t<ADFun<double> > sphess */
typedef sphess_t<ADFUN > sphess;

/*
  Suppose we have a mapping F:R^n->R^m which may be written as F=F1+...+Fk.
  Suppose we have tapes Fi:R^n->R^mi representing Fi with identical domain
  but with *reduced range dimension* (because some range components of Fi
  does not depend on any of the domain variables).
  Based on these tape chunks construct an object behaving just like the 
  corresponding full taped version of F.
 */
template <class Type>
struct parallelADFun : ADFUN { /* Inheritance just so that compiler wont complain about missing members */
  typedef ADFUN Base;
  /* Following five members must be defined by constructor.
     Outer vectors are indexed by the chunk number.
     E.g. for tape number i vecind[i] is a vector of numbers in the 
     interval [0,1,...,range-1] telling how to embed this tapes range
     in the full range.
  */
  int ntapes;
  vector<Base*> vecpf;
  vector<vector<size_t> > vecind;
  size_t domain;
  size_t range;
  /* Following members are optional */
  vector<sphess* > H_;
  /* row and column indices */
  vector<int> veci;
  vector<int> vecj;
  /* Constructor:
     In the case of a vector of ADFun pointers we assume that
     they all have equal domain and range dimensions.
   */
  void CTOR(vector<Base*> vecpf_) {
    size_t n=vecpf_.size();
    ntapes=n;
    vecpf.resize(n);
    for(size_t i=0;i<n;i++)vecpf[i]=vecpf_[i];
    domain=vecpf[0]->Domain();
    range=vecpf[0]->Range();
    vecind.resize(n);
    for(size_t i=0;i<n;i++){
      vecind(i).resize(range);
      for(size_t j=0;j<range;j++){
	vecind(i)[j]=j;
      }
    }
  }
  parallelADFun(vector<Base*> vecpf_) {
    CTOR(vecpf_);
  }
  parallelADFun(const std::vector<Base> &vecf) {
    vector<Base*> vecpf(vecf.size());
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (int i=0; i<vecpf.size(); i++) {
      vecpf[i] = new Base(vecf[i]);
    }
    CTOR(vecpf);
  }
  /* Constructor:
     In the case of a vector of sphess pointers the range dimensions are allowed
     to differ. Based on sparseness pattern represented by each tape we must
     compute "vecind" (for each tape), formally by matching the individual 
     sparseness patterns in a numbering of the full sparseness pattern (the union).
   */
  parallelADFun(vector<sphess* > H){
    H_=H;
    domain=H[0]->pf->Domain();
    int n=H.size();
    ntapes=n;
    vecpf.resize(n);
    vecind.resize(n);
    for(int i=0;i<n;i++)vecpf[i]=H[i]->pf;
    size_t kmax=0;
    for(int i=0;i<n;i++){
      //vecind[i]=(vector<size_t>(H[i]->i))+(vector<size_t>(H[i]->j))*domain;
      vecind[i]=((H[i]->i).cast<size_t>())+((H[i]->j).cast<size_t>())*domain;
      kmax+=vecind[i].size();
    }
    veci.resize(kmax);vecj.resize(kmax);
    vector<int> pos(n); /* keep track of positions in individual index vectors */
    for(int i=0;i<n;i++){pos(i)=0;};
    if(config.trace.parallel) std::cout << "Hessian number of non-zeros:\n";
    for(int i=0;i<n;i++){
      if(config.trace.parallel) std::cout << "nnz = " << vecind(i).size() << "\n";
    };
    vector<size_t> value(n); /* value corresponding to pos */
    int k=0; /* Incremented for each unique value */
    size_t m; /* Hold current minimum value */
    size_t inf=-1; /* size_t is unsigned - so -1 should give the largest possible size_t... */
    int rowk=-1,colk=-1; /* -Wall */
    while(true){
      for(int i=0;i<n;i++){if(pos(i)<vecind(i).size())value(i)=vecind(i)[pos(i)];else value(i)=inf;}
      m=value(0);
      for(int i=0;i<n;i++){if(value(i)<m)m=value(i);}
      if(m==inf)break;
      for(int i=0;i<n;i++){
	if(value(i)==m){
	  vecind(i)[pos(i)]=k;
	  rowk=(H[i]->i)[pos(i)];
	  colk=(H[i]->j)[pos(i)];
	  pos(i)++;
	}
      }
      veci[k]=rowk;
      vecj[k]=colk;
      k++;
    }
    range=k;
    //veci.resize(k);vecj.resize(k);
    veci.conservativeResize(k);vecj.conservativeResize(k);
  };
  /* Destructor */
  ~parallelADFun(){
    if(config.trace.parallel) std::cout << "Free parallelADFun object.\n";
    for(int i=0;i<vecpf.size();i++){
      delete vecpf[i];
    }
  }
  /* Convenience: convert this object to sphess like object */
  sphess_t<parallelADFun<double> > convert(){
    sphess_t<parallelADFun<double> > ans(this,veci,vecj);
    return ans;
  }  
  /* Subset of vector x to indices of tape number "tapeid" */
  template <typename VectorBase>
  VectorBase subset(const VectorBase& x, size_t tapeid, int p=1){
    VectorBase y;
    y.resize(vecind(tapeid).size()*p);
    for(int i=0;i<(int)y.size()/p;i++)
      for(int j=0;j<p;j++)
	{y[i*p+j]=x[vecind(tapeid)[i]*p+j];}
    return y;
  }
  /* Inverse operation of the subset above */
  template <typename VectorBase>
  void addinsert(VectorBase& x, const VectorBase& y, size_t tapeid, int p=1){
    for(int i=0;i<(int)y.size()/p;i++)
      for(int j=0;j<p;j++)
	{x[vecind(tapeid)[i]*p+j]+=y[i*p+j];}
  }

  /* Overload methods */
  size_t Domain(){return domain;}
  size_t Range(){return range;}

#ifdef CPPAD_FRAMEWORK
  /* p=order, p+1 taylorcoefficients per variable, x=domain vector 
     x contains p'th order taylor coefficients of input (length n). 
     Output contains (p+1)'th order taylor coefficients (length m).
     =====> output = vector of length m (m=range dim)
   */
  template <typename VectorBase>
  VectorBase Forward(size_t p, const VectorBase& x, std::ostream& s = std::cout){
    vector<VectorBase> ans(ntapes);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0;i<ntapes;i++)ans(i) = vecpf(i)->Forward(p,x);
    VectorBase out(range);
    for(size_t i=0;i<range;i++)out(i)=0;
    for(int i=0;i<ntapes;i++)addinsert(out,ans(i),i);
    return out;
  }
  /* p=number of taylor coefs per variable (fastest running in output vector).
     v=rangeweight vector. Can be either of length m or m*p. (m=range dim)
     output=vector of length p*n (n=domain dim).
  */
  template <typename VectorBase>
  VectorBase Reverse(size_t p, const VectorBase &v){
    vector<VectorBase> ans(ntapes);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0;i<ntapes;i++)ans(i) = vecpf(i)->Reverse(p,subset(v,i));
    VectorBase out(p*domain); 
    for(size_t i=0;i<p*domain;i++)out(i)=0;
    for(int i=0;i<ntapes;i++)out=out+ans(i);
    return out;
  }
  template <typename VectorBase>
  VectorBase Jacobian(const VectorBase &x){
    vector<VectorBase> ans(ntapes);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0;i<ntapes;i++)ans(i) = vecpf(i)->Jacobian(x);
    VectorBase out( domain * range ); // domain fastest running
    out.setZero();
    for(int i=0;i<ntapes;i++)addinsert(out,ans(i),i,domain);
    return out;
  }
  template <typename VectorBase>
  VectorBase Hessian(const VectorBase &x, size_t rangecomponent){
    vector<VectorBase> ans(ntapes);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0;i<ntapes;i++)ans(i) = vecpf(i)->Hessian(x,rangecomponent);
    VectorBase out( domain * domain );
    out.setZero();
    for(int i=0;i<ntapes;i++)addinsert(out,ans(i),i,domain*domain);
    return out;
  }
  /* optimize ADFun object */
  void optimize(){
    if(config.trace.optimize)std::cout << "Optimizing parallel tape... ";
#ifdef _OPENMP
#pragma omp parallel for if (config.optimize.parallel)
#endif
    for(int i=0;i<ntapes;i++)vecpf(i)->optimize();
    if(config.trace.optimize)std::cout << "Done\n";
  }
#endif // CPPAD_FRAMEWORK

#ifdef TMBAD_FRAMEWORK
  void unset_tail() {
    for(int i=0; i<ntapes; i++) vecpf(i) -> unset_tail();
  }
  void set_tail(const std::vector<TMBad::Index> &r) {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0; i<ntapes; i++) vecpf(i) -> set_tail(r);
  }
  void force_update() {
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0; i<ntapes; i++) vecpf(i) -> force_update();
  }
  vector<double> operator()(const std::vector<double> &x) {
    vector<vector<double> > ans(ntapes);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0; i<ntapes; i++)
      ans(i) = vector<double>(vecpf(i)->operator()(x));
    vector<double> out(range);
    out.setZero();
    for(int i=0; i<ntapes; i++) addinsert(out, ans(i), i);
    return out;
  }
  vector<double> Jacobian(const std::vector<double> &x) {
    vector<vector<double> > ans(ntapes);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0; i<ntapes; i++)
      ans(i) = vector<double>(vecpf(i)->Jacobian(x));
    vector<double> out( domain * range ); // domain fastest running
    out.setZero();
    for(int i=0; i<ntapes; i++) addinsert(out, ans(i), i, domain);
    return out;
  }
  vector<double> Jacobian(const std::vector<double> &x,
                          const std::vector<bool> &keep_x,
                          const std::vector<bool> &keep_y ) {
    vector<vector<double> > ans(ntapes);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0; i<ntapes; i++)
      ans(i) = vector<double>(vecpf(i)->Jacobian(x, keep_x, subset(keep_y, i)));
    // Calculate indices into row space of resulting jacobian (subset)
    vector<vector<size_t> > vecind2(vecind.size());
    std::vector<size_t> remap = TMBad::cumsum0<size_t> (keep_y);
    for(int i=0; i<ntapes; i++) {
      std::vector<bool>
        sub = subset(keep_y, i); // Bool mask into vecind(i)
      std::vector<size_t>
        vecind_i(vecind(i));
      std::vector<size_t>
        vecind_i_sub = TMBad::subset(vecind_i, sub); // remaining vecind(i)
      std::vector<size_t>
        vecind_i_sub_remap = TMBad::subset(remap, vecind_i_sub); // Remap
      vecind2(i) = vector<size_t> (vecind_i_sub_remap);
    }
    // Fill into result matrix
    int dim_x = std::count(keep_x.begin(), keep_x.end(), true);
    int dim_y = std::count(keep_y.begin(), keep_y.end(), true);
    vector<double> out( dim_x * dim_y );
    out.setZero();
    std::swap(vecind, vecind2);
    for (int i=0; i<ntapes; i++) addinsert(out, ans(i), i, dim_x);
    std::swap(vecind, vecind2);
    return out;
  }
  vector<double> Jacobian(const std::vector<double> &x,
                          const vector<double> &w) {
    vector<vector<double> > ans(ntapes);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0; i<ntapes; i++)
      ans(i) = vector<double>(vecpf(i)->Jacobian(x, subset(w, i)));
    vector<double> out(domain);
    out.setZero();
    for(int i=0; i<ntapes; i++) out = out + ans(i);
    return out;
  }
  // Used by tmbstan (tmb_forward and tmb_reverse)
  // Assuming one-dimensional output.
  template<class Vector>
  Vector forward(const Vector &x) {
    vector<Vector> ans(ntapes);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0; i<ntapes; i++) ans(i) = vecpf(i)->forward(x);
    Vector out(1);
    out.setZero();
    for(int i=0; i<ntapes; i++) out = out + ans(i);
    return out;
  }
  template<class Vector>
  Vector reverse(const Vector &w) {
    vector<Vector> ans(ntapes);
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(int i=0; i<ntapes; i++) ans(i) = vecpf(i)->reverse(w);
    Vector out(domain);
    out.setZero();
    for(int i=0; i<ntapes; i++) out = out + ans(i);
    return out;
  }
#endif // TMBAD_FRAMEWORK
};

