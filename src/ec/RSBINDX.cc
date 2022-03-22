#include "RSBINDX.hh"

RSBINDX::RSBINDX(int n, int k, int w, int opt, vector<string> param) {
  _n = n;
  _k = k;
  _w = w;
  _opt = opt;

  _m = _n - _k;
  memset(_encode_matrix, 0, (_k+_m) * _k * sizeof(int));
  if(RSBINDX_DEBUG_ENABLE) cout << "RSBINDX::constructor ends" << endl; 
}

ECDAG* RSBINDX::Encode() {
  ECDAG* ecdag = new ECDAG();
  vector<int> data;
  vector<int> code;
  for (int i=0; i<_k; i++) data.push_back(i);
  for (int i=_k; i<_n; i++) code.push_back(i);
  if (RSBINDX_DEBUG_ENABLE) {
    cout << "RSBINDX::Encode.data:";
    for (int i=0; i<data.size(); i++) cout << " " << data[i];
    cout << endl;
    cout << "RSBINDX::Encode.code:";
    for (int i=0; i<code.size(); i++) cout << " " << code[i];
    cout << endl;
  }
  
  generate_matrix(_encode_matrix, _n, _k, 8);
  for (int i=0; i<_m; i++) {
    vector<int> coef;
    for (int j=0; j<_k; j++) {
      coef.push_back(_encode_matrix[(i+_k)*_k+j]);
    }
    ecdag->Join(code[i], data, coef);
  }
  if (_m > 1) ecdag->BindX(code);
  return ecdag;
}

ECDAG* RSBINDX::Decode(vector<int> from, vector<int> to) {
  ECDAG* ecdag = new ECDAG();
  generate_matrix(_encode_matrix, _n, _k, 8);
  vector<int> data;
  int _select_matrix[_k*_k];
  for (int i=0; i<_k; i++) {
    data.push_back(from[i]);
    int sidx = from[i];
    memcpy(_select_matrix + i * _k,
           _encode_matrix + sidx * _k,
 	   sizeof(int) * _k);
  }
  int _invert_matrix[_k*_k];

  jerasure_invert_matrix(_select_matrix, _invert_matrix, _k, _k); 
  for (int i=0; i<to.size(); i++) {
    int ridx = to[i];
    int _select_vector[_k];
    memcpy(_select_vector,
           _encode_matrix + ridx * _k,
	   _k * sizeof(int));
    int* _coef_vector = jerasure_matrix_multiply(
        _select_vector, _invert_matrix, 1, _k, _k, _k, 8);
    vector<int> coef;
    for (int i=0; i<_k; i++) coef.push_back(_coef_vector[i]);
    ecdag->Join(ridx, data, coef);
  }
  return ecdag;
}

void RSBINDX::Place(vector<vector<int>>& group) {
}

void RSBINDX::generate_matrix(int* matrix, int rows, int cols, int w) {
  int k = cols;
  int n = rows;
  int m = n - k;

  memset(matrix, 0, rows * cols * sizeof(int));
  for(int i=0; i<k; i++) {
    matrix[i*k+i] = 1;
  }

  for (int i=0; i<m; i++) {
    int tmp = 1;
    for (int j=0; j<k; j++) {
      matrix[(i+k)*cols+j] = tmp;
      tmp = Computation::singleMulti(tmp, i+1, w);
    }
  }
}
