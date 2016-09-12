#include <iostream>
#include <sstream>

#include <Rcpp.h>

#include <geos/geom/Geometry.h>
#include <geos/io/WKBReader.h>
#include <geos/operation/distance/DistanceOp.h>
#include <geos/operation/relate/RelateOp.h>
#include <geos/operation/valid/IsValidOp.h>
#include <geos/geom/IntersectionMatrix.h>
#include <geos/geom/prep/PreparedGeometryFactory.h>

#include "wkb.h"

geos::geom::Geometry *geometry_from_raw(Rcpp::RawVector wkb) {
	std::istringstream s;
	std::istringstream& str(s);
	str.rdbuf()->pubsetbuf( (char *) &(wkb[0]), wkb.size());
	geos::io::WKBReader r;
	return(r.read(str));
}

std::vector<geos::geom::Geometry *> geometries_from_sfc(Rcpp::List sfc) {
	double precision = sfc.attr("precision");
	Rcpp::List wkblst = CPL_write_wkb(sfc, false, native_endian(), "XY", precision);
	std::vector<geos::geom::Geometry *> g(sfc.length());
	for (int i = 0; i < wkblst.length(); i++)
		g[i] = geometry_from_raw(wkblst[i]);
	return(g);
}

Rcpp::NumericVector get_dim(double dim0, double dim1) {
	Rcpp::NumericVector dim(2);
	dim(0) = dim0;
	dim(1) = dim1;
	return(dim);
}

Rcpp::IntegerVector get_which(Rcpp::LogicalVector row) {
	int j = 0;
	for (int i = 0; i < row.length(); i++)
		if (row(i))
			j++;
	Rcpp::IntegerVector ret(j);
	for (int i = 0, j = 0; i < row.length(); i++)
		if (row(i))
			ret(j++) = i + 1; // R is 1-based
	return(ret);
}

// [[Rcpp::export]]
Rcpp::List CPL_geos_binop(Rcpp::List sfc0, Rcpp::List sfc1, std::string op, double par = 0.0, 
		bool sparse = true) {
	std::vector<geos::geom::Geometry *> gmv0 = geometries_from_sfc(sfc0);
	std::vector<geos::geom::Geometry *> gmv1 = geometries_from_sfc(sfc1);

	using namespace Rcpp;
	if (op == "relate") { // character return matrix:
		Rcpp::CharacterVector out(sfc0.length() * sfc1.length());
		for (int i = 0; i < sfc0.length(); i++) {
			for (int j = 0; j < sfc1.length(); j++) {
				static geos::geom::IntersectionMatrix* im;
				im = geos::operation::relate::RelateOp::relate(gmv0[i], gmv1[j]);
				out[j * sfc0.length() + i] = im->toString(); // TODO: does this copy the string?
			}
		}
		out.attr("dim") = get_dim(sfc0.length(), sfc1.length());
		return(Rcpp::List::create(out));
	} 
	if (op == "distance") { // double return matrix:
		Rcpp::NumericMatrix out(sfc0.length(), sfc1.length());
		for (int i = 0; i < sfc0.length(); i++)
			for (int j = 0; j < sfc1.length(); j++)
				out(i,j) = gmv0[i]->distance(gmv1[j]);
		return(Rcpp::List::create(out));
	}
	// other cases: boolean return matrix, either dense or sparse
	Rcpp::LogicalMatrix densemat;
	if (! sparse)  // allocate:
		densemat = Rcpp::LogicalMatrix(sfc0.length(), sfc1.length());
	Rcpp::List sparsemat(sfc0.length());
	for (int i = 0; i < sfc0.length(); i++) { // row
	// TODO: speed up contains, containsproperly, covers, and intersects with prepared geometry i
		Rcpp::LogicalVector rowi(sfc1.length()); 
		if (op == "intersects")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->intersects(gmv1[j]);
		else if (op == "disjoint")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->disjoint(gmv1[j]);
		else if (op == "touches")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->touches(gmv1[j]);
		else if (op == "crosses")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->crosses(gmv1[j]);
		else if (op == "within")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->within(gmv1[j]);
		else if (op == "contains")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->contains(gmv1[j]);
		else if (op == "overlaps")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->overlaps(gmv1[j]);
		else if (op == "equals")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->equals(gmv1[j]);
		else if (op == "covers")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->covers(gmv1[j]);
		else if (op == "coveredBy")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->coveredBy(gmv1[j]);
		else if (op == "equalsExact")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->equalsExact(gmv1[j], par);
		else if (op == "isWithinDistance")
			for (int j = 0; j < sfc1.length(); j++) 
				rowi(j) = gmv0[i]->isWithinDistance(gmv1[j], par);
		else
			throw std::range_error("wrong value for op");
		if (! sparse)
			densemat(i,_) = rowi;
		else
			sparsemat[i] = get_which(rowi);
	}
	if (sparse)
		return(sparsemat);
	else
		return(Rcpp::List::create(densemat));
}

// [[Rcpp::export]]
Rcpp::LogicalVector CPL_geos_is_valid(Rcpp::List sfc) { 
	std::vector<geos::geom::Geometry *> gmv = geometries_from_sfc(sfc);
	Rcpp::LogicalVector out(sfc.length());
	for (int i; i < sfc.length(); i++)
		out[i] = geos::operation::valid::IsValidOp::isValid(*gmv[i]);
	return(out);
}

// [[Rcpp::export]]
std::string CPL_geos_version(bool b = false) {
	return(geos::geom::geosversion());
}

// [[Rcpp::export]]
Rcpp::NumericMatrix CPL_geos_dist(Rcpp::List sfc0, Rcpp::List sfc1) {
	Rcpp::NumericMatrix out = CPL_geos_binop(sfc0, sfc1, "distance", 0.0, false)[0];
	return(out);
}

// [[Rcpp::export]]
Rcpp::CharacterVector CPL_geos_relate(Rcpp::List sfc0, Rcpp::List sfc1) {
	Rcpp::CharacterVector out = CPL_geos_binop(sfc0, sfc1, "relate", 0.0, false)[0];
	return(out);	
}