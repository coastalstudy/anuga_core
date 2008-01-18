// Python - C extension for quantity module.
//
// To compile (Python2.3):
//  gcc -c util_ext.c -I/usr/include/python2.3 -o util_ext.o -Wall -O
//  gcc -shared util_ext.o  -o util_ext.so
//
// See the module quantity.py
//
//
// Ole Nielsen, GA 2004

#include "Python.h"
#include "Numeric/arrayobject.h"
#include "math.h"

//Shared code snippets
#include "util_ext.h"
//#include "quantity_ext.h" //Obsolete



int _compute_gradients(int N,
			double* centroids,
			double* centroid_values,
			long* number_of_boundaries,
			long* surrogate_neighbours,
			double* a,
			double* b){

	int i, k, k0, k1, k2, index3;
	double x0, x1, x2, y0, y1, y2, q0, q1, q2; //, det;


	for (k=0; k<N; k++) {
		index3 = 3*k;

		if (number_of_boundaries[k] < 2) {
			// Two or three true neighbours

			// Get indices of neighbours (or self when used as surrogate)
			// k0, k1, k2 = surrogate_neighbours[k,:]

			k0 = surrogate_neighbours[index3 + 0];
			k1 = surrogate_neighbours[index3 + 1];
			k2 = surrogate_neighbours[index3 + 2];


			if (k0 == k1 || k1 == k2) return -1;

			// Get data
			q0 = centroid_values[k0];
			q1 = centroid_values[k1];
			q2 = centroid_values[k2];

			x0 = centroids[k0*2]; y0 = centroids[k0*2+1];
			x1 = centroids[k1*2]; y1 = centroids[k1*2+1];
			x2 = centroids[k2*2]; y2 = centroids[k2*2+1];

			// Gradient
			_gradient(x0, y0, x1, y1, x2, y2, q0, q1, q2, &a[k], &b[k]);

		} else if (number_of_boundaries[k] == 2) {
			// One true neighbour

			// Get index of the one neighbour
			i=0; k0 = k;
			while (i<3 && k0==k) {
				k0 = surrogate_neighbours[index3 + i];
				i++;
			}
			if (k0 == k) return -1;

			k1 = k; //self

			// Get data
			q0 = centroid_values[k0];
			q1 = centroid_values[k1];

			x0 = centroids[k0*2]; y0 = centroids[k0*2+1];
			x1 = centroids[k1*2]; y1 = centroids[k1*2+1];

			// Two point gradient
			_gradient2(x0, y0, x1, y1, q0, q1, &a[k], &b[k]);

		}
		//    else:
		//        #No true neighbours -
		//        #Fall back to first order scheme
	}
	return 0;
}



int _extrapolate_from_gradient(int N,
		 double* centroids,
		 double* centroid_values,
		 double* vertex_coordinates,
		 double* vertex_values,
		 double* edge_values,
		 double* a,
		 double* b) {

	int k, k2, k3, k6;
	double x, y, x0, y0, x1, y1, x2, y2;

	for (k=0; k<N; k++){
		k6 = 6*k;
		k3 = 3*k;
		k2 = 2*k;

		// Centroid coordinates
		x = centroids[k2]; y = centroids[k2+1];

		// vertex coordinates
		// x0, y0, x1, y1, x2, y2 = X[k,:]
		x0 = vertex_coordinates[k6 + 0];
		y0 = vertex_coordinates[k6 + 1];
		x1 = vertex_coordinates[k6 + 2];
		y1 = vertex_coordinates[k6 + 3];
		x2 = vertex_coordinates[k6 + 4];
		y2 = vertex_coordinates[k6 + 5];

		// Extrapolate to Vertices
		vertex_values[k3+0] = centroid_values[k] + a[k]*(x0-x) + b[k]*(y0-y);
		vertex_values[k3+1] = centroid_values[k] + a[k]*(x1-x) + b[k]*(y1-y);
		vertex_values[k3+2] = centroid_values[k] + a[k]*(x2-x) + b[k]*(y2-y);

		// Extrapolate to Edges (midpoints)
		edge_values[k3+0] = 0.5*(vertex_values[k3 + 1]+vertex_values[k3 + 2]);
		edge_values[k3+1] = 0.5*(vertex_values[k3 + 2]+vertex_values[k3 + 0]);
		edge_values[k3+2] = 0.5*(vertex_values[k3 + 0]+vertex_values[k3 + 1]);

	}
	return 0;
}


int _limit_vertices_by_all_neighbours(int N, double beta,
		     double* centroid_values,
		     double* vertex_values,
		     double* edge_values,
		     long*   neighbours) {

	int i, k, k2, k3, k6;
	long n;
	double qmin, qmax, qn, qc;
	double dq, dqa[3], phi, r;

	for (k=0; k<N; k++){
		k6 = 6*k;
		k3 = 3*k;
		k2 = 2*k;

		qc = centroid_values[k];
		qmin = qc;
		qmax = qc;

		for (i=0; i<3; i++) {
		    n = neighbours[k3+i];
		    if (n >= 0) {
			qn = centroid_values[n]; //Neighbour's centroid value

			qmin = min(qmin, qn);
			qmax = max(qmax, qn);
		    }
		}

		phi = 1.0;
		for (i=0; i<3; i++) {    
		    r = 1.0;
      
		    dq = vertex_values[k3+i] - qc;    //Delta between vertex and centroid values
		    dqa[i] = dq;                      //Save dq for use in updating vertex values
      
		    if (dq > 0.0) r = (qmax - qc)/dq;
		    if (dq < 0.0) r = (qmin - qc)/dq;      
  
		    
		    phi = min( min(r*beta, 1.0), phi);    
		}
    
		//Update vertex and edge values using phi limiter
		vertex_values[k3+0] = qc + phi*dqa[0];
		vertex_values[k3+1] = qc + phi*dqa[1];
		vertex_values[k3+2] = qc + phi*dqa[2];
		
		edge_values[k3+0] = 0.5*(vertex_values[k3+1] + vertex_values[k3+2]);
		edge_values[k3+1] = 0.5*(vertex_values[k3+2] + vertex_values[k3+0]);
		edge_values[k3+2] = 0.5*(vertex_values[k3+0] + vertex_values[k3+1]);

	}

	return 0;
}

int _limit_edges_by_all_neighbours(int N, double beta,
		     double* centroid_values,
		     double* vertex_values,
		     double* edge_values,
		     long*   neighbours) {

	int i, k, k2, k3, k6;
	long n;
	double qmin, qmax, qn, qc;
	double dq, dqa[3], phi, r;

	for (k=0; k<N; k++){
		k6 = 6*k;
		k3 = 3*k;
		k2 = 2*k;

		qc = centroid_values[k];
		qmin = qc;
		qmax = qc;

		for (i=0; i<3; i++) {
		    n = neighbours[k3+i];
		    if (n >= 0) {
			qn = centroid_values[n]; //Neighbour's centroid value

			qmin = min(qmin, qn);
			qmax = max(qmax, qn);
		    }
		}

		phi = 1.0;
		for (i=0; i<3; i++) {    
		    r = 1.0;
      
		    dq = edge_values[k3+i] - qc;     //Delta between edge and centroid values
		    dqa[i] = dq;                      //Save dq for use in updating vertex values
      
		    if (dq > 0.0) r = (qmax - qc)/dq;
		    if (dq < 0.0) r = (qmin - qc)/dq;      
  
		    
		    phi = min( min(r*beta, 1.0), phi);    
		}
    
		//Update vertex and edge values using phi limiter
		edge_values[k3+0] = qc + phi*dqa[0];
		edge_values[k3+1] = qc + phi*dqa[1];
		edge_values[k3+2] = qc + phi*dqa[2];
		
		vertex_values[k3+0] = edge_values[k3+1] + edge_values[k3+2] - edge_values[k3+0];
		vertex_values[k3+1] = edge_values[k3+2] + edge_values[k3+0] - edge_values[k3+1];
		vertex_values[k3+2] = edge_values[k3+0] + edge_values[k3+1] - edge_values[k3+2];

	}

	return 0;
}


int _limit_edges_by_neighbour(int N, double beta,
		     double* centroid_values,
		     double* vertex_values,
		     double* edge_values,
		     long*   neighbours) {

	int i, k, k2, k3, k6;
	long n;
	double qmin, qmax, qn, qc;
	double dq, dqa[3], phi, r;

	for (k=0; k<N; k++){
		k6 = 6*k;
		k3 = 3*k;
		k2 = 2*k;

		qc = centroid_values[k];
		phi = 1.0;

		for (i=0; i<3; i++) {
		    dq = edge_values[k3+i] - qc;     //Delta between edge and centroid values
		    dqa[i] = dq;                      //Save dq for use in updating vertex values

		    n = neighbours[k3+i];
		    if (n >= 0) {
			qn = centroid_values[n]; //Neighbour's centroid value

			qmin = min(qc, qn);
			qmax = max(qc, qn);

			r = 1.0;
      
			if (dq > 0.0) r = (qmax - qc)/dq;
			if (dq < 0.0) r = (qmin - qc)/dq;      
		    
			phi = min( min(r*beta, 1.0), phi);    
		    }
		}


		//Update edge and vertex values using phi limiter
		edge_values[k3+0] = qc + phi*dqa[0];
		edge_values[k3+1] = qc + phi*dqa[1];
		edge_values[k3+2] = qc + phi*dqa[2];
		
		vertex_values[k3+0] = edge_values[k3+1] + edge_values[k3+2] - edge_values[k3+0];
		vertex_values[k3+1] = edge_values[k3+2] + edge_values[k3+0] - edge_values[k3+1];
		vertex_values[k3+2] = edge_values[k3+0] + edge_values[k3+1] - edge_values[k3+2];

	}

	return 0;
}


int _limit_gradient_by_neighbour(int N, double beta,
		     double* centroid_values,
		     double* vertex_values,
		     double* edge_values,
		     double* x_gradient,
		     double* y_gradient,
		     long*   neighbours) {

	int i, k, k2, k3, k6;
	long n;
	double qmin, qmax, qn, qc;
	double dq, dqa[3], phi, r;

	for (k=0; k<N; k++){
		k6 = 6*k;
		k3 = 3*k;
		k2 = 2*k;

		qc = centroid_values[k];
		phi = 1.0;

		for (i=0; i<3; i++) {
		    dq = edge_values[k3+i] - qc;     //Delta between edge and centroid values
		    dqa[i] = dq;                      //Save dq for use in updating vertex values

		    n = neighbours[k3+i];
		    if (n >= 0) {
			qn = centroid_values[n]; //Neighbour's centroid value

			qmin = min(qc, qn);
			qmax = max(qc, qn);

			r = 1.0;
      
			if (dq > 0.0) r = (qmax - qc)/dq;
			if (dq < 0.0) r = (qmin - qc)/dq;      
		    
			phi = min( min(r*beta, 1.0), phi);    
		    }
		}


		//Update edge and vertex values using phi limiter
		edge_values[k3+0] = qc + phi*dqa[0];
		edge_values[k3+1] = qc + phi*dqa[1];
		edge_values[k3+2] = qc + phi*dqa[2];
		
		vertex_values[k3+0] = edge_values[k3+1] + edge_values[k3+2] - edge_values[k3+0];
		vertex_values[k3+1] = edge_values[k3+2] + edge_values[k3+0] - edge_values[k3+1];
		vertex_values[k3+2] = edge_values[k3+0] + edge_values[k3+1] - edge_values[k3+2];

	}

	return 0;
}



int _interpolate_from_vertices_to_edges(int N,
		 double* vertex_values,
		 double* edge_values) {

	int k, k3;
	double q0, q1, q2;


	for (k=0; k<N; k++) {
		k3 = 3*k;

		q0 = vertex_values[k3 + 0];
		q1 = vertex_values[k3 + 1];
		q2 = vertex_values[k3 + 2];

		edge_values[k3 + 0] = 0.5*(q1+q2);
		edge_values[k3 + 1] = 0.5*(q0+q2);
		edge_values[k3 + 2] = 0.5*(q0+q1);
	}
	return 0;
}


int _interpolate_from_edges_to_vertices(int N,
		 double* vertex_values,
		 double* edge_values) {

	int k, k3;
	double e0, e1, e2;


	for (k=0; k<N; k++) {
		k3 = 3*k;

		e0 = edge_values[k3 + 0];
		e1 = edge_values[k3 + 1];
		e2 = edge_values[k3 + 2];

		vertex_values[k3 + 0] = e1 + e2 - e0;
		vertex_values[k3 + 1] = e2 + e0 - e1;
		vertex_values[k3 + 2] = e0 + e1 - e2;
	}
	return 0;
}

int _backup_centroid_values(int N,
			    double* centroid_values,
			    double* centroid_backup_values) {
    // Backup centroid values


    int k;

    for (k=0; k<N; k++) {
	centroid_backup_values[k] = centroid_values[k];
    }


    return 0;
}


int _saxpy_centroid_values(int N,
			   double a,
			   double b,
			   double* centroid_values,
			   double* centroid_backup_values) {
    // Saxby centroid values


    int k;


    for (k=0; k<N; k++) {
	centroid_values[k] = a*centroid_values[k] + b*centroid_backup_values[k];
    }


    return 0;
}


int _update(int N,
	    double timestep,
	    double* centroid_values,
	    double* explicit_update,
	    double* semi_implicit_update) {
	// Update centroid values based on values stored in
	// explicit_update and semi_implicit_update as well as given timestep


	int k;
	double denominator, x;


	// Divide semi_implicit update by conserved quantity
	for (k=0; k<N; k++) {
		x = centroid_values[k];
		if (x == 0.0) {
			semi_implicit_update[k] = 0.0;
		} else {
			semi_implicit_update[k] /= x;
		}
	}


	// Semi implicit updates
	for (k=0; k<N; k++) {
		denominator = 1.0 - timestep*semi_implicit_update[k];
		if (denominator == 0.0) {
			return -1;
		} else {
			//Update conserved_quantities from semi implicit updates
			centroid_values[k] /= denominator;
		}
	}


	// Explicit updates
	for (k=0; k<N; k++) {
		centroid_values[k] += timestep*explicit_update[k];
	}


	// MH080605 set semi_implicit_update[k] to 0.0 here, rather than in update_conserved_quantities.py
	for (k=0;k<N;k++){
		semi_implicit_update[k]=0.0;
	}

	return 0;
}


int _average_vertex_values(int N,
			   long* vertex_value_indices,
			   long* number_of_triangles_per_node,
			   double* vertex_values, 
			   double* A) {
  // Average vertex values to obtain one value per node

  int i, index; 
  int k = 0; //Track triangles touching each node
  int current_node = 0;
  double total = 0.0;

  for (i=0; i<N; i++) {
  
    // if (current_node == N) {
    //   printf("Current node exceeding number of nodes (%d)", N);    
    //   return 1;
    // }
    
    index = vertex_value_indices[i];
    k += 1;
            
    // volume_id = index / 3
    // vertex_id = index % 3
    // total += self.vertex_values[volume_id, vertex_id]
    total += vertex_values[index];
      
    // printf("current_node=%d, index=%d, k=%d, total=%f\n", current_node, index, k, total);
    if (number_of_triangles_per_node[current_node] == k) {
      A[current_node] = total/k;
                
      // Move on to next node
      total = 0.0;
      k = 0;
      current_node += 1;
    }
  }
			   
  return 0;
}


/////////////////////////////////////////////////
// Gateways to Python
PyObject *update(PyObject *self, PyObject *args) {
  // FIXME (Ole): It would be great to turn this text into a Python DOC string

  /*"""Update centroid values based on values stored in
    explicit_update and semi_implicit_update as well as given timestep

    Function implementing forcing terms must take on argument
    which is the domain and they must update either explicit
    or implicit updates, e,g,:

    def gravity(domain):
        ....
        domain.quantities['xmomentum'].explicit_update = ...
        domain.quantities['ymomentum'].explicit_update = ...



    Explicit terms must have the form

        G(q, t)

    and explicit scheme is

       q^{(n+1}) = q^{(n)} + delta_t G(q^{n}, n delta_t)


    Semi implicit forcing terms are assumed to have the form

       G(q, t) = H(q, t) q

    and the semi implicit scheme will then be

      q^{(n+1}) = q^{(n)} + delta_t H(q^{n}, n delta_t) q^{(n+1})

  */

	PyObject *quantity;
	PyArrayObject *centroid_values, *explicit_update, *semi_implicit_update;

	double timestep;
	int N, err;


	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "Od", &quantity, &timestep)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: update could not parse input");
	  return NULL;
	}

	centroid_values = get_consecutive_array(quantity, "centroid_values");
	explicit_update = get_consecutive_array(quantity, "explicit_update");
	semi_implicit_update = get_consecutive_array(quantity, "semi_implicit_update");

	N = centroid_values -> dimensions[0];

	err = _update(N, timestep,
		      (double*) centroid_values -> data,
		      (double*) explicit_update -> data,
		      (double*) semi_implicit_update -> data);


	if (err != 0) {
	  PyErr_SetString(PyExc_RuntimeError,
			  "Zero division in semi implicit update - call Stephen :)");
	  return NULL;
	}

	// Release and return
	Py_DECREF(centroid_values);
	Py_DECREF(explicit_update);
	Py_DECREF(semi_implicit_update);

	return Py_BuildValue("");
}


PyObject *backup_centroid_values(PyObject *self, PyObject *args) {

	PyObject *quantity;
	PyArrayObject *centroid_values, *centroid_backup_values;

	int N, err;


	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "O", &quantity)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: backup_centroid_values could not parse input");
	  return NULL;
	}

	centroid_values        = get_consecutive_array(quantity, "centroid_values");
	centroid_backup_values = get_consecutive_array(quantity, "centroid_backup_values");

	N = centroid_values -> dimensions[0];

	err = _backup_centroid_values(N,
		      (double*) centroid_values -> data,
		      (double*) centroid_backup_values -> data);


	// Release and return
	Py_DECREF(centroid_values);
	Py_DECREF(centroid_backup_values);

	return Py_BuildValue("");
}

PyObject *saxpy_centroid_values(PyObject *self, PyObject *args) {

	PyObject *quantity;
	PyArrayObject *centroid_values, *centroid_backup_values;

	double a,b;
	int N, err;


	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "Odd", &quantity, &a, &b)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: saxpy_centroid_values could not parse input");
	  return NULL;
	}

	centroid_values        = get_consecutive_array(quantity, "centroid_values");
	centroid_backup_values = get_consecutive_array(quantity, "centroid_backup_values");

	N = centroid_values -> dimensions[0];

	err = _saxpy_centroid_values(N,a,b,
		      (double*) centroid_values -> data,
		      (double*) centroid_backup_values -> data);


	// Release and return
	Py_DECREF(centroid_values);
	Py_DECREF(centroid_backup_values);

	return Py_BuildValue("");
}


PyObject *interpolate_from_vertices_to_edges(PyObject *self, PyObject *args) {
        //
        //Compute edge values from vertex values using linear interpolation
        // 
	
	PyObject *quantity;
	PyArrayObject *vertex_values, *edge_values;

	int N, err;

	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "O", &quantity)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: interpolate_from_vertices_to_edges could not parse input");
	  return NULL;
	}
	
	vertex_values = get_consecutive_array(quantity, "vertex_values");
	edge_values = get_consecutive_array(quantity, "edge_values");

	N = vertex_values -> dimensions[0];

	err = _interpolate_from_vertices_to_edges(N,
			   (double*) vertex_values -> data,
			   (double*) edge_values -> data);

	if (err != 0) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "Interpolate could not be computed");
	  return NULL;
	}

	// Release and return
	Py_DECREF(vertex_values);
	Py_DECREF(edge_values);

	return Py_BuildValue("");
}


PyObject *interpolate_from_edges_to_vertices(PyObject *self, PyObject *args) {
        //
        //Compute vertex values from edge values using linear interpolation
        // 
	
	PyObject *quantity;
	PyArrayObject *vertex_values, *edge_values;

	int N, err;

	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "O", &quantity)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: interpolate_from_edges_to_vertices could not parse input");
	  return NULL;
	}
	
	vertex_values = get_consecutive_array(quantity, "vertex_values");
	edge_values = get_consecutive_array(quantity, "edge_values");

	N = vertex_values -> dimensions[0];

	err = _interpolate_from_edges_to_vertices(N,
			   (double*) vertex_values -> data,
			   (double*) edge_values -> data);

	if (err != 0) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "Interpolate could not be computed");
	  return NULL;
	}

	// Release and return
	Py_DECREF(vertex_values);
	Py_DECREF(edge_values);

	return Py_BuildValue("");
}


PyObject *average_vertex_values(PyObject *self, PyObject *args) {

	PyArrayObject 
	  *vertex_value_indices, 
	  *number_of_triangles_per_node,
	  *vertex_values,
	  *A;
	

	int N, err;

	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "OOOO",
			      &vertex_value_indices, 
			      &number_of_triangles_per_node,
			      &vertex_values, 
			      &A)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: average_vertex_values could not parse input");
	  return NULL;
	}
	
	N = vertex_value_indices -> dimensions[0];
	// printf("Got parameters, N=%d\n", N);
	err = _average_vertex_values(N,
				     (long*) vertex_value_indices -> data,
				     (long*) number_of_triangles_per_node -> data,
				     (double*) vertex_values -> data, 
				     (double*) A -> data);

	//printf("Error %d", err);
	if (err != 0) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "average_vertex_values could not be computed");
	  return NULL;
	}

	return Py_BuildValue("");
}



/* PyObject *compute_gradients(PyObject *self, PyObject *args) { */
/*   //"""Compute gradients of triangle surfaces defined by centroids of */
/*   //neighbouring volumes. */
/*   //If one edge is on the boundary, use own centroid as neighbour centroid. */
/*   //If two or more are on the boundary, fall back to first order scheme. */
/*   //""" */


/* 	PyObject *quantity, *domain, *R; */
/* 	PyArrayObject */
/* 		*centroids,            //Coordinates at centroids */
/* 		*centroid_values,      //Values at centroids */
/* 		*number_of_boundaries, //Number of boundaries for each triangle */
/* 		*surrogate_neighbours, //True neighbours or - if one missing - self */
/* 		*a, *b;                //Return values */

/* 	int dimensions[1], N, err; */

/* 	// Convert Python arguments to C */
/* 	if (!PyArg_ParseTuple(args, "O", &quantity)) { */
/* 	  PyErr_SetString(PyExc_RuntimeError,  */
/* 			  "quantity_ext.c: compute_gradients could not parse input");	 */
/* 	  return NULL; */
/* 	} */

/* 	domain = PyObject_GetAttrString(quantity, "domain"); */
/* 	if (!domain) { */
/* 	  PyErr_SetString(PyExc_RuntimeError,  */
/* 			  "compute_gradients could not obtain domain object from quantity"); */
/* 	  return NULL; */
/* 	} */

/* 	// Get pertinent variables */

/* 	centroids = get_consecutive_array(domain, "centroid_coordinates"); */
/* 	centroid_values = get_consecutive_array(quantity, "centroid_values"); */
/* 	surrogate_neighbours = get_consecutive_array(domain, "surrogate_neighbours"); */
/* 	number_of_boundaries = get_consecutive_array(domain, "number_of_boundaries"); */

/* 	N = centroid_values -> dimensions[0]; */

/* 	// Release */
/* 	Py_DECREF(domain); */

/* 	// Allocate space for return vectors a and b (don't DECREF) */
/* 	dimensions[0] = N; */
/* 	a = (PyArrayObject *) PyArray_FromDims(1, dimensions, PyArray_DOUBLE); */
/* 	b = (PyArrayObject *) PyArray_FromDims(1, dimensions, PyArray_DOUBLE); */



/* 	err = _compute_gradients(N, */
/* 			(double*) centroids -> data, */
/* 			(double*) centroid_values -> data, */
/* 			(long*) number_of_boundaries -> data, */
/* 			(long*) surrogate_neighbours -> data, */
/* 			(double*) a -> data, */
/* 			(double*) b -> data); */

/* 	if (err != 0) { */
/* 	  PyErr_SetString(PyExc_RuntimeError, "Gradient could not be computed"); */
/* 	  return NULL; */
/* 	} */

/* 	// Release */
/* 	Py_DECREF(centroids); */
/* 	Py_DECREF(centroid_values); */
/* 	Py_DECREF(number_of_boundaries); */
/* 	Py_DECREF(surrogate_neighbours); */

/* 	// Build result, release and return */
/* 	R = Py_BuildValue("OO", PyArray_Return(a), PyArray_Return(b)); */
/* 	Py_DECREF(a); */
/* 	Py_DECREF(b); */
/* 	return R; */
/* } */



PyObject *extrapolate_from_gradient(PyObject *self, PyObject *args) {

	PyObject *quantity, *domain;
	PyArrayObject
	    *centroids,            //Coordinates at centroids
	    *centroid_values,      //Values at centroids
	    *vertex_coordinates,   //Coordinates at vertices
	    *vertex_values,        //Values at vertices
	    *edge_values,          //Values at edges
	    *number_of_boundaries, //Number of boundaries for each triangle
	    *surrogate_neighbours, //True neighbours or - if one missing - self
	    *x_gradient,           //x gradient
	    *y_gradient;           //y gradient

	//int N, err;
	//int dimensions[1];
	int N, err;
	//double *a, *b;  //Gradients

	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "O", &quantity)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "extrapolate_gradient could not parse input");	
	  return NULL;
	}

	domain = PyObject_GetAttrString(quantity, "domain");
	if (!domain) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "extrapolate_gradient could not obtain domain object from quantity");	
	  return NULL;
	}

	// Get pertinent variables
	centroids            = get_consecutive_array(domain,   "centroid_coordinates");
	centroid_values      = get_consecutive_array(quantity, "centroid_values");
	surrogate_neighbours = get_consecutive_array(domain,   "surrogate_neighbours");
	number_of_boundaries = get_consecutive_array(domain,   "number_of_boundaries");
	vertex_coordinates   = get_consecutive_array(domain,   "vertex_coordinates");
	vertex_values        = get_consecutive_array(quantity, "vertex_values");
	edge_values          = get_consecutive_array(quantity, "edge_values");
	x_gradient           = get_consecutive_array(quantity, "x_gradient");
	y_gradient           = get_consecutive_array(quantity, "y_gradient");

	N = centroid_values -> dimensions[0];

	// Release
	Py_DECREF(domain);

	//Allocate space for return vectors a and b (don't DECREF)
	//dimensions[0] = N;
	//a = (PyArrayObject *) PyArray_FromDims(1, dimensions, PyArray_DOUBLE);
	//b = (PyArrayObject *) PyArray_FromDims(1, dimensions, PyArray_DOUBLE);

	//FIXME: Odd that I couldn't use normal arrays
	//Allocate space for return vectors a and b (don't DECREF)
	//a = (double*) malloc(N * sizeof(double));
	//if (!a) return NULL;
	//b = (double*) malloc(N * sizeof(double));
	//if (!b) return NULL;


/* 	err = _compute_gradients(N, */
/* 			(double*) centroids -> data, */
/* 			(double*) centroid_values -> data, */
/* 			(long*) number_of_boundaries -> data, */
/* 			(long*) surrogate_neighbours -> data, */
/* 			(double*) x_gradient -> data, */
/* 			(double*) y_gradient -> data); */

/* 	if (err != 0) { */
/* 	  PyErr_SetString(PyExc_RuntimeError, "Gradient could not be computed"); */
/* 	  return NULL; */
/* 	} */

	err = _extrapolate_from_gradient(N,
			(double*) centroids -> data,
			(double*) centroid_values -> data,
			(double*) vertex_coordinates -> data,
			(double*) vertex_values -> data,
			(double*) edge_values -> data,
			(double*) x_gradient -> data,
			(double*) y_gradient -> data);


	if (err != 0) {
	  PyErr_SetString(PyExc_RuntimeError,
			  "Internal function _extrapolate failed");
	  return NULL;
	}



	// Release
	Py_DECREF(centroids);
	Py_DECREF(centroid_values);
	Py_DECREF(number_of_boundaries);
	Py_DECREF(surrogate_neighbours);
	Py_DECREF(vertex_coordinates);
	Py_DECREF(vertex_values);
	Py_DECREF(edge_values);
	Py_DECREF(x_gradient);
	Py_DECREF(y_gradient);

	return Py_BuildValue("");
}



PyObject *compute_gradients(PyObject *self, PyObject *args) {

	PyObject *quantity, *domain;
	PyArrayObject
	    *centroids,            //Coordinates at centroids
	    *centroid_values,      //Values at centroids
	    *vertex_coordinates,   //Coordinates at vertices
	    *vertex_values,        //Values at vertices
	    *edge_values,          //Values at edges
	    *number_of_boundaries, //Number of boundaries for each triangle
	    *surrogate_neighbours, //True neighbours or - if one missing - self
	    *x_gradient,           //x gradient
	    *y_gradient;           //y gradient

	//int N, err;
	//int dimensions[1];
	int N, err;
	//double *a, *b;  //Gradients

	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "O", &quantity)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "compute_gradients could not parse input");	
	  return NULL;
	}

	domain = PyObject_GetAttrString(quantity, "domain");
	if (!domain) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "compute_gradients could not obtain domain object from quantity");	
	  return NULL;
	}

	// Get pertinent variables
	centroids            = get_consecutive_array(domain,   "centroid_coordinates");
	centroid_values      = get_consecutive_array(quantity, "centroid_values");
	surrogate_neighbours = get_consecutive_array(domain,   "surrogate_neighbours");
	number_of_boundaries = get_consecutive_array(domain,   "number_of_boundaries");
	vertex_coordinates   = get_consecutive_array(domain,   "vertex_coordinates");
	vertex_values        = get_consecutive_array(quantity, "vertex_values");
	edge_values          = get_consecutive_array(quantity, "edge_values");
	x_gradient           = get_consecutive_array(quantity, "x_gradient");
	y_gradient           = get_consecutive_array(quantity, "y_gradient");

	N = centroid_values -> dimensions[0];

	// Release
	Py_DECREF(domain);

	//Allocate space for return vectors a and b (don't DECREF)
	//dimensions[0] = N;
	//a = (PyArrayObject *) PyArray_FromDims(1, dimensions, PyArray_DOUBLE);
	//b = (PyArrayObject *) PyArray_FromDims(1, dimensions, PyArray_DOUBLE);

	//FIXME: Odd that I couldn't use normal arrays
	//Allocate space for return vectors a and b (don't DECREF)
	//a = (double*) malloc(N * sizeof(double));
	//if (!a) return NULL;
	//b = (double*) malloc(N * sizeof(double));
	//if (!b) return NULL;


	err = _compute_gradients(N,
			(double*) centroids -> data,
			(double*) centroid_values -> data,
			(long*) number_of_boundaries -> data,
			(long*) surrogate_neighbours -> data,
			(double*) x_gradient -> data,
			(double*) y_gradient -> data);

	if (err != 0) {
	  PyErr_SetString(PyExc_RuntimeError, "Gradient could not be computed");
	  return NULL;
	}

/* 	err = _extrapolate(N, */
/* 			(double*) centroids -> data, */
/* 			(double*) centroid_values -> data, */
/* 			(double*) vertex_coordinates -> data, */
/* 			(double*) vertex_values -> data, */
/* 			(double*) edge_values -> data, */
/* 			(double*) x_gradient -> data, */
/* 			(double*) y_gradient -> data); */


/* 	if (err != 0) { */
/* 	  PyErr_SetString(PyExc_RuntimeError, */
/* 			  "Internal function _extrapolate failed"); */
/* 	  return NULL; */
/* 	} */



	// Release
	Py_DECREF(centroids);
	Py_DECREF(centroid_values);
	Py_DECREF(number_of_boundaries);
	Py_DECREF(surrogate_neighbours);
	Py_DECREF(vertex_coordinates);
	Py_DECREF(vertex_values);
	Py_DECREF(edge_values);
	Py_DECREF(x_gradient);
	Py_DECREF(y_gradient);

	return Py_BuildValue("");
}



PyObject *limit_old(PyObject *self, PyObject *args) {
  //Limit slopes for each volume to eliminate artificial variance
  //introduced by e.g. second order extrapolator

  //This is an unsophisticated limiter as it does not take into
  //account dependencies among quantities.

  //precondition:
  //  vertex values are estimated from gradient
  //postcondition:
  //  vertex values are updated
  //

	PyObject *quantity, *domain, *Tmp;
	PyArrayObject
	    *qv, //Conserved quantities at vertices
	    *qc, //Conserved quantities at centroids
	    *neighbours;

	int k, i, n, N, k3;
	double beta_w; //Safety factor
	double *qmin, *qmax, qn;

	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "O", &quantity)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_old could not parse input");
	  return NULL;
	}

	domain = PyObject_GetAttrString(quantity, "domain");
	if (!domain) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_old could not obtain domain object from quantity");		  	
	  
	  return NULL;
	}

	//neighbours = (PyArrayObject*) PyObject_GetAttrString(domain, "neighbours");
	neighbours = get_consecutive_array(domain, "neighbours");

	// Get safety factor beta_w
	Tmp = PyObject_GetAttrString(domain, "beta_w");
	if (!Tmp) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_old could not obtain beta_w object from domain");		  	
	  
	  return NULL;
	}	

	beta_w = PyFloat_AsDouble(Tmp);

	Py_DECREF(Tmp);
	Py_DECREF(domain);


	qc = get_consecutive_array(quantity, "centroid_values");
	qv = get_consecutive_array(quantity, "vertex_values");


	N = qc -> dimensions[0];

	// Find min and max of this and neighbour's centroid values
	qmin = malloc(N * sizeof(double));
	qmax = malloc(N * sizeof(double));
	for (k=0; k<N; k++) {
		k3=k*3;

		qmin[k] = ((double*) qc -> data)[k];
		qmax[k] = qmin[k];

		for (i=0; i<3; i++) {
			n = ((long*) neighbours -> data)[k3+i];
			if (n >= 0) {
				qn = ((double*) qc -> data)[n]; //Neighbour's centroid value

				qmin[k] = min(qmin[k], qn);
				qmax[k] = max(qmax[k], qn);
			}
			//qmin[k] = max(qmin[k],0.5*((double*) qc -> data)[k]);
			//qmax[k] = min(qmax[k],2.0*((double*) qc -> data)[k]);
		}
	}

	// Call underlying routine
	_limit_old(N, beta_w, (double*) qc -> data, (double*) qv -> data, qmin, qmax);

	free(qmin);
	free(qmax);
	return Py_BuildValue("");
}


PyObject *limit_vertices_by_all_neighbours(PyObject *self, PyObject *args) {
  //Limit slopes for each volume to eliminate artificial variance
  //introduced by e.g. second order extrapolator

  //This is an unsophisticated limiter as it does not take into
  //account dependencies among quantities.

  //precondition:
  //  vertex values are estimated from gradient
  //postcondition:
  //  vertex and edge values are updated
  //

	PyObject *quantity, *domain, *Tmp;
	PyArrayObject
	    *vertex_values,   //Conserved quantities at vertices
	    *centroid_values, //Conserved quantities at centroids
	    *edge_values,     //Conserved quantities at edges
	    *neighbours;

	double beta_w; //Safety factor
	int N, err;


	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "O", &quantity)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_by_vertex could not parse input");
	  return NULL;
	}

	domain = PyObject_GetAttrString(quantity, "domain");
	if (!domain) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_by_vertex could not obtain domain object from quantity");		  	
	  
	  return NULL;
	}

	// Get safety factor beta_w
	Tmp = PyObject_GetAttrString(domain, "beta_w");
	if (!Tmp) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_by_vertex could not obtain beta_w object from domain");		  	
	  
	  return NULL;
	}	


	// Get pertinent variables
	neighbours       = get_consecutive_array(domain, "neighbours");
	centroid_values  = get_consecutive_array(quantity, "centroid_values");
	vertex_values    = get_consecutive_array(quantity, "vertex_values");
	edge_values      = get_consecutive_array(quantity, "edge_values");
	beta_w           = PyFloat_AsDouble(Tmp);


	N = centroid_values -> dimensions[0];

	err = _limit_vertices_by_all_neighbours(N, beta_w,
						(double*) centroid_values -> data,
						(double*) vertex_values -> data,
						(double*) edge_values -> data,
						(long*)   neighbours -> data);
	
	if (err != 0) {
	  PyErr_SetString(PyExc_RuntimeError,
			  "Internal function _limit_by_vertex failed");
	  return NULL;
	}	


	// Release
	Py_DECREF(neighbours);
	Py_DECREF(centroid_values);
	Py_DECREF(vertex_values);
	Py_DECREF(edge_values);
	Py_DECREF(Tmp);


	return Py_BuildValue("");
}



PyObject *limit_edges_by_all_neighbours(PyObject *self, PyObject *args) {
  //Limit slopes for each volume to eliminate artificial variance
  //introduced by e.g. second order extrapolator

  //This is an unsophisticated limiter as it does not take into
  //account dependencies among quantities.

  //precondition:
  //  vertex values are estimated from gradient
  //postcondition:
  //  vertex and edge values are updated
  //

	PyObject *quantity, *domain, *Tmp;
	PyArrayObject
	    *vertex_values,   //Conserved quantities at vertices
	    *centroid_values, //Conserved quantities at centroids
	    *edge_values,     //Conserved quantities at edges
	    *neighbours;

	double beta_w; //Safety factor
	int N, err;


	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "O", &quantity)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_edges_by_all_neighbours could not parse input");
	  return NULL;
	}

	domain = PyObject_GetAttrString(quantity, "domain");
	if (!domain) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_edges_by_all_neighbours could not obtain domain object from quantity");		  	
	  
	  return NULL;
	}

	// Get safety factor beta_w
	Tmp = PyObject_GetAttrString(domain, "beta_w");
	if (!Tmp) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_edges_by_all_neighbours could not obtain beta_w object from domain");		  	
	  
	  return NULL;
	}	


	// Get pertinent variables
	neighbours       = get_consecutive_array(domain, "neighbours");
	centroid_values  = get_consecutive_array(quantity, "centroid_values");
	vertex_values    = get_consecutive_array(quantity, "vertex_values");
	edge_values      = get_consecutive_array(quantity, "edge_values");
	beta_w           = PyFloat_AsDouble(Tmp);


	N = centroid_values -> dimensions[0];

	err = _limit_edges_by_all_neighbours(N, beta_w,
					     (double*) centroid_values -> data,
					     (double*) vertex_values -> data,
					     (double*) edge_values -> data,
					     (long*)   neighbours -> data);

	if (err != 0) {
	  PyErr_SetString(PyExc_RuntimeError,
			  "Internal function _limit_by_vertex failed");
	  return NULL;
	}	


	// Release
	Py_DECREF(neighbours);
	Py_DECREF(centroid_values);
	Py_DECREF(vertex_values);
	Py_DECREF(edge_values);
	Py_DECREF(Tmp);


	return Py_BuildValue("");
}


PyObject *limit_edges_by_neighbour(PyObject *self, PyObject *args) {
  //Limit slopes for each volume to eliminate artificial variance
  //introduced by e.g. second order extrapolator

  //This is an unsophisticated limiter as it does not take into
  //account dependencies among quantities.

  //precondition:
  //  vertex values are estimated from gradient
  //postcondition:
  //  vertex and edge values are updated
  //

	PyObject *quantity, *domain, *Tmp;
	PyArrayObject
	    *vertex_values,   //Conserved quantities at vertices
	    *centroid_values, //Conserved quantities at centroids
	    *edge_values,     //Conserved quantities at edges
	    *neighbours;

	double beta_w; //Safety factor
	int N, err;


	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "O", &quantity)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_edges_by_neighbour could not parse input");
	  return NULL;
	}

	domain = PyObject_GetAttrString(quantity, "domain");
	if (!domain) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_edges_by_neighbour could not obtain domain object from quantity");		  	
	  
	  return NULL;
	}

	// Get safety factor beta_w
	Tmp = PyObject_GetAttrString(domain, "beta_w");
	if (!Tmp) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_by_vertex could not obtain beta_w object from domain");		  	
	  
	  return NULL;
	}	


	// Get pertinent variables
	neighbours       = get_consecutive_array(domain, "neighbours");
	centroid_values  = get_consecutive_array(quantity, "centroid_values");
	vertex_values    = get_consecutive_array(quantity, "vertex_values");
	edge_values      = get_consecutive_array(quantity, "edge_values");
	beta_w           = PyFloat_AsDouble(Tmp);


	N = centroid_values -> dimensions[0];

	err = _limit_edges_by_neighbour(N, beta_w,
					(double*) centroid_values -> data,
					(double*) vertex_values -> data,
					(double*) edge_values -> data,
					(long*)   neighbours -> data);
	
	if (err != 0) {
	  PyErr_SetString(PyExc_RuntimeError,
			  "Internal function _limit_by_vertex failed");
	  return NULL;
	}	


	// Release
	Py_DECREF(neighbours);
	Py_DECREF(centroid_values);
	Py_DECREF(vertex_values);
	Py_DECREF(edge_values);
	Py_DECREF(Tmp);


	return Py_BuildValue("");
}


PyObject *limit_gradient_by_neighbour(PyObject *self, PyObject *args) {
  //Limit slopes for each volume to eliminate artificial variance
  //introduced by e.g. second order extrapolator

  //This is an unsophisticated limiter as it does not take into
  //account dependencies among quantities.

  //precondition:
  //  vertex values are estimated from gradient
  //postcondition:
  //  vertex and edge values are updated
  //

	PyObject *quantity, *domain, *Tmp;
	PyArrayObject
	    *vertex_values,   //Conserved quantities at vertices
	    *centroid_values, //Conserved quantities at centroids
	    *edge_values,     //Conserved quantities at edges
	    *x_gradient,
	    *y_gradient,
	    *neighbours;

	double beta_w; //Safety factor
	int N, err;


	// Convert Python arguments to C
	if (!PyArg_ParseTuple(args, "O", &quantity)) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_gradient_by_neighbour could not parse input");
	  return NULL;
	}

	domain = PyObject_GetAttrString(quantity, "domain");
	if (!domain) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_gradient_by_neighbour could not obtain domain object from quantity");		  	
	  
	  return NULL;
	}

	// Get safety factor beta_w
	Tmp = PyObject_GetAttrString(domain, "beta_w");
	if (!Tmp) {
	  PyErr_SetString(PyExc_RuntimeError, 
			  "quantity_ext.c: limit_gradient_by_neighbour could not obtain beta_w object from domain");		  	
	  
	  return NULL;
	}	


	// Get pertinent variables
	neighbours       = get_consecutive_array(domain, "neighbours");
	centroid_values  = get_consecutive_array(quantity, "centroid_values");
	vertex_values    = get_consecutive_array(quantity, "vertex_values");
	edge_values      = get_consecutive_array(quantity, "edge_values");
	x_gradient       = get_consecutive_array(quantity, "x_gradient");
	y_gradient       = get_consecutive_array(quantity, "y_gradient");

	beta_w           = PyFloat_AsDouble(Tmp);


	N = centroid_values -> dimensions[0];

	err = _limit_gradient_by_neighbour(N, beta_w,
					(double*) centroid_values -> data,
					(double*) vertex_values -> data,
					(double*) edge_values -> data,
					(double*) x_gradient -> data,
					(double*) y_gradient -> data,
					(long*)   neighbours -> data);
	
	if (err != 0) {
	  PyErr_SetString(PyExc_RuntimeError,
			  "Internal function _limit_gradient_by_neighbour failed");
	  return NULL;
	}	


	// Release
	Py_DECREF(neighbours);
	Py_DECREF(centroid_values);
	Py_DECREF(vertex_values);
	Py_DECREF(edge_values);
	Py_DECREF(x_gradient);
	Py_DECREF(y_gradient);
	Py_DECREF(Tmp);


	return Py_BuildValue("");
}


// Method table for python module
static struct PyMethodDef MethodTable[] = {
	{"limit_old", limit_old, METH_VARARGS, "Print out"},
	{"limit_vertices_by_all_neighbours", limit_vertices_by_all_neighbours, METH_VARARGS, "Print out"},
	{"limit_edges_by_all_neighbours", limit_edges_by_all_neighbours, METH_VARARGS, "Print out"},
	{"limit_edges_by_neighbour", limit_edges_by_neighbour, METH_VARARGS, "Print out"},
	{"limit_gradient_by_neighbour", limit_gradient_by_neighbour, METH_VARARGS, "Print out"},
	{"update", update, METH_VARARGS, "Print out"},
	{"backup_centroid_values", backup_centroid_values, METH_VARARGS, "Print out"},
	{"saxpy_centroid_values", saxpy_centroid_values, METH_VARARGS, "Print out"},
	{"compute_gradients", compute_gradients, METH_VARARGS, "Print out"},
	{"extrapolate_from_gradient", extrapolate_from_gradient,
		METH_VARARGS, "Print out"},
	{"interpolate_from_vertices_to_edges",
		interpolate_from_vertices_to_edges,
		METH_VARARGS, "Print out"},
	{"interpolate_from_edges_to_vertices",
		interpolate_from_edges_to_vertices,
		METH_VARARGS, "Print out"},
	{"average_vertex_values", average_vertex_values, METH_VARARGS, "Print out"},		
	{NULL, NULL, 0, NULL}   // sentinel
};

// Module initialisation
void initquantity_ext(void){
  Py_InitModule("quantity_ext", MethodTable);

  import_array(); // Necessary for handling of NumPY structures
}
