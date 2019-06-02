#include "Model.h"

using namespace std;

Model::Model(VectorXf *response, int maxTerms, CSMatrix *csMatrix) {
	this->response = response;
	this->maxTerms = maxTerms;
	this->csMatrix = csMatrix;
	this->tests = response->getLength();
	
	// allocate memory for the coefficients vector
	coefVec = new float[maxTerms];
	
	// allocate memory for the residuals vector
	resiVec = new float[tests];
	
	// allocate memory for the model response
	modelResponse = new float[tests];
	
	// add the intercept
	terms = 1;
	hTermIndex = new TermIndex;
	hTermIndex->termIndex = 0;
	hTermIndex->next = NULL;
	
	// this is a one line way to get the same intercept above
	leastSquares();

}

Model::Model(Model *model) {
	this->response = model->response;
	this->maxTerms = model->maxTerms;
	this->csMatrix = model->csMatrix;
	this->rSquared = model->rSquared;
	this->terms = model->terms;
	this->tests = response->getLength();
	
	// allocate memory for the coefficients vector and copy
	coefVec = new float[maxTerms];
	for (int coef_i = 0; coef_i < maxTerms; coef_i++) {
		coefVec[coef_i] = model->coefVec[coef_i];
	}
	
	// allocate memory for the residuals vector
	resiVec = new float[tests];
	for (int resi_i = 0; resi_i < tests; resi_i++) {
		resiVec[resi_i] = model->resiVec[resi_i];
	}
	
	// allocate memory for the model response
	modelResponse = new float[tests];
	for (int resp_i = 0; resp_i < tests; resp_i++) {
		modelResponse[resp_i] = model->modelResponse[resp_i];
	}
	
	// copy term index list
	TermIndex **destTermIndex = &hTermIndex;
	for (TermIndex *pTermIndex = model->hTermIndex; pTermIndex != NULL;
			pTermIndex = pTermIndex->next) {
		(*destTermIndex) = new TermIndex;
		(*destTermIndex)->termIndex = pTermIndex->termIndex;
		(*destTermIndex)->next = NULL;
		destTermIndex = &(*destTermIndex)->next;
	}
}

void Model::printModelFactors() {
	int factor1i, factor2i, level1, level2;
	
	// print out the number of terms
	cout << "Model with " << terms << " terms" << endl;
	
	// print out the headers
	cout << setw(15) << right << "Coefficient" << " | " << "Term" << endl;
	cout << setw(15) << setfill('-') << "" << " | " <<
			setw(15) << setfill('-') << "" << setfill(' ') << endl;
	
	// print out each term
	int term_i = 0;
	for (TermIndex *pTermIndex = hTermIndex; pTermIndex != NULL; pTermIndex = pTermIndex->next) {
		// print out the coefficient
		int termIndex = pTermIndex->termIndex;
		cout << setw(15) << right << coefVec[term_i++] << " | ";
		
		// print out the factor names and level names
		cout << csMatrix->getColName(csMatrix->getCol(termIndex)) << endl;
		
	}
	
	// calculate adjusted r-squared (terms - 2 to not include the intercept)
	float adjustedRSquared = 1 - (1 - rSquared) * (tests - 1) / (tests - terms - 2);
	
	// print model r-squared
	if (rSquared == 1) cout << "Perfect Model!!!" << endl;
	cout << "R-Squared: " << rSquared << endl;
	cout << "Adjusted R-Squared: " << adjustedRSquared << endl;
}

void Model::leastSquares() {
	
	// used when accessing CS Matrix
	CSCol *csCol;
	
	// used when accessing the master design matrix
	float *designCol;
	
	// used when looping through term indices
	TermIndex *pTermIndex;
	
	// check dimensions
	if (terms > tests) {
		cout << "Cols (terms) cannot be more than rows (tests) to perform QR" << endl;
		return;
	}
	
	// hydrate design matrix (is hydrate the right word? I like it)
	pTermIndex = hTermIndex;
	for (int col_i = 0; col_i < terms; col_i++) {
		if (workSpace->designMatrix[pTermIndex->termIndex] == NULL) {
			char **levelMatrix = csMatrix->getLocatingArray()->getLevelMatrix();
			
			csCol = csMatrix->getCol(pTermIndex->termIndex);
			
			// allocate some memory for this column of the design matrix and populate it
			designCol = new float[tests];
			
			bool rowData;
			for (int row_i = 0; row_i < tests; row_i++) {	// populate every row
				
				rowData = true; // start with true, perform AND operation
				
				for (int setting_i = 0; setting_i < csCol->factors; setting_i++) {
					
					// swap it around if needed
					if (levelMatrix[row_i][csCol->setting[setting_i].factor_i] >=
							csCol->setting[setting_i].index &&
						levelMatrix[row_i][csCol->setting[setting_i].factor_i] <
							csCol->setting[setting_i].index + csCol->setting[setting_i].levelsInGroup) {
						rowData = !rowData;
					}
					
				}
				
				designCol[row_i] = (rowData ? ENTRY_A : ENTRY_B);
			}
			workSpace->designMatrix[pTermIndex->termIndex] = designCol;
		}
		
		pTermIndex = pTermIndex->next;
	}
	
	// do QR here (column by column)
	pTermIndex = hTermIndex;
	for (int col_i = 0; col_i < terms; col_i++) {
		
		designCol = workSpace->designMatrix[pTermIndex->termIndex];
		// assign initial column A[col_i] to work vector
		for (int row_i = 0; row_i < tests; row_i++) {
			workSpace->workVec[row_i] = designCol[row_i];
		}
		
		// subtract appropriate other vectors
		float dotProd;
		for (int row_i = 0; row_i < col_i; row_i++) {
			// find the dot product of A[:][col_i] and Q[:][row_i]
			dotProd = 0;
			for (int dotrow_i = 0; dotrow_i < tests; dotrow_i++)
				dotProd += designCol[dotrow_i] * workSpace->dataQ[dotrow_i][row_i];
			
			// assign the dot product to the R matrix
			workSpace->dataR[row_i][col_i] = dotProd;
			
			// perform the necessary subtraction
			for (int subrow_i = 0; subrow_i < tests; subrow_i++)
				workSpace->workVec[subrow_i] -= dotProd * workSpace->dataQ[subrow_i][row_i];
			
		}
		
		// find the dot product of workVec and workVec
		dotProd = 0;
		for (int dotrow_i = 0; dotrow_i < tests; dotrow_i++)
			dotProd += workSpace->workVec[dotrow_i] * workSpace->workVec[dotrow_i];
		dotProd = sqrt(dotProd);
		
		// assign the dot product to the R matrix
		workSpace->dataR[col_i][col_i] = dotProd;
		
		// assign the normalized column to Q
		if (dotProd == 0) {
			for (int row_i = 0; row_i < tests; row_i++)
				workSpace->dataQ[row_i][col_i] = 0;
		} else {
			for (int row_i = 0; row_i < tests; row_i++)
				workSpace->dataQ[row_i][col_i] = workSpace->workVec[row_i] / dotProd;
		}
		
		pTermIndex = pTermIndex->next;
	}
	
	/*
	cout << "Q matrix" << endl;
	for (int row_i = 0; row_i < tests; row_i++) {
		for (int col_i = 0; col_i < terms; col_i++) {
			cout << workSpace->dataQ[row_i][col_i] << "\t";
		}
		cout << endl;
	}
	
	cout << "R matrix" << endl;
	for (int row_i = 0; row_i < terms; row_i++) {
		for (int col_i = 0; col_i < terms; col_i++) {
			cout << workSpace->dataR[row_i][col_i] << "\t";
		}
		cout << endl;
	}
	*/
	
	/* m >> n
	** A is (m by n)
	** Q is (m by n)
	** R is (n by n)
	** b is (m by 1)
	** Q-transpose is (n by m)
	**
	*/
	// We now have QRx = b
	
	// perform workVec = Q-transpose * b
	
	// first zero out the work vector up until n (or cols)
	for (int row_i = 0; row_i < terms; row_i++)
		workSpace->workVec[row_i] = 0;
	
	// we go column 1st because Q is transposed
	float *responseData = response->getData();
	for (int col_i = 0; col_i < tests; col_i++) {
		for (int row_i = 0; row_i < terms; row_i++) {
			// row and col are swapped because of the transpose
			workSpace->workVec[row_i] += workSpace->dataQ[col_i][row_i] * responseData[col_i];
		}
	}
	
	// We now have Rx = workVec but R is upper triangular (n by n)
	float rowSolution;
	for (int row_i = terms - 1; row_i >= 0; row_i--) {
		
		// initialize row solution
		rowSolution = workSpace->workVec[row_i];
		
		// subtract other parts of LHS of equation
		for (int col_i = terms - 1; col_i > row_i; col_i--) {
			rowSolution -= workSpace->dataR[row_i][col_i] * coefVec[col_i];
		}
		
		// divide for final row solution
		if (workSpace->dataR[row_i][row_i] == 0) {
			coefVec[row_i] = 0;
		} else {
			coefVec[row_i] = rowSolution / workSpace->dataR[row_i][row_i];
		}
		
	}
	
	// coefVec holds the least squares coefficients
	//cout << "coefficient vector" << endl;
	//for (int term_i = 0; term_i < terms; term_i++) {
	//	cout << coefVec[term_i] << " ";
	//}
	//cout << endl;
	
	// calculate residuals and r-squared
	float SSres = 0;
	for (int row_i = 0; row_i < tests; row_i++) {
		modelResponse[row_i] = 0;
	}
	
	// find model response
	pTermIndex = hTermIndex;
	for (int term_i = 0; term_i < terms; term_i++) {
		designCol = workSpace->designMatrix[pTermIndex->termIndex];
		for (int row_i = 0; row_i < tests; row_i++) {
			modelResponse[row_i] += designCol[row_i] * coefVec[term_i];
		}
		pTermIndex = pTermIndex->next;
	}
	
	// find residuals and SSres
	for (int row_i = 0; row_i < tests; row_i++) {
		resiVec[row_i] = responseData[row_i] - modelResponse[row_i];
		
		// add the square of residual to SSres
		SSres += resiVec[row_i] * resiVec[row_i];
	}
	
	// find r-squared
	rSquared = 1 - (SSres / response->getSStot());
	
}

// get the residuals vector for this model
float *Model::getResiVec() {
	return resiVec;
}

float Model::getRSquared() {
	return rSquared;
}

// check if term is part of the model
bool Model::termExists(int col_i) {
	for (TermIndex *pTermIndex = hTermIndex; pTermIndex != NULL; pTermIndex = pTermIndex->next) {
		if (pTermIndex->termIndex == col_i) {
			// term was found!
			return true;
		}
	}
	// term never found
	return false;
}

// add a term (col_i of csMatrix) to the model. Returns true if added successfully
bool Model::addTerm(int col_i) {
	
	TermIndex **pTermIndex = &hTermIndex;
	for (; *pTermIndex != NULL; pTermIndex = &(*pTermIndex)->next) {
		
		if ((*pTermIndex)->termIndex == col_i) {
			
			// already exists so cannot insert
			return false;
			
		} else if ((*pTermIndex)->termIndex > col_i) {
			
			// insert within the list
			TermIndex *termIndex = new TermIndex;
			termIndex->termIndex = col_i;
			termIndex->next = (*pTermIndex);
			(*pTermIndex) = termIndex;
			terms++;
			return true;
			
		}
		
	}
	
	// create a new term index at the end
	TermIndex *termIndex = new TermIndex;
	termIndex->termIndex = col_i;
	termIndex->next = NULL;
	(*pTermIndex) = termIndex;
	terms++;
	
	return true;
	
}

// remove a term from the model
bool Model::removeTerm(int col_i) {
	// loop through the term index list
	for (TermIndex **pTermIndex = &hTermIndex; *pTermIndex != NULL; pTermIndex = &(*pTermIndex)->next) {
		// check if it equals the term to be removed (col_i)
		if ((*pTermIndex)->termIndex == col_i) {
			// take the current term index out of the list
			TermIndex *removed = (*pTermIndex);
			(*pTermIndex) = removed->next;
			
			// decrease the number of terms
			terms--;
			
			// free its memory
			delete removed;
			
			return true;
		}
	}
	
	// term to remove was not found
	return false;
}

int Model::getTerms() {
	return terms;
}

// static
void Model::setupWorkSpace(int rows, int modelCols, int csCols) {
	// allocate initial memory
	workSpace = new WorkSpace;
	
	// allocate memory for Q
	workSpace->dataQ = new float*[rows];
	for (int row_i = 0; row_i < rows; row_i++)
		workSpace->dataQ[row_i] = new float[modelCols];
	
	// allocate memory for R
	workSpace->dataR = new float*[modelCols];
	for (int row_i = 0; row_i < modelCols; row_i++)
		workSpace->dataR[row_i] = new float[modelCols];
	
	// allocate memory for work vector
	workSpace->workVec = new float[rows];
	
	// allocate memory for design matrix
	workSpace->designMatrix = new float*[csCols];
	for (int col_i = 0; col_i < csCols; col_i++)
		workSpace->designMatrix[col_i] = NULL;
}

void Model::countOccurrences(Occurrence *occurrence) {
	// count occurrences for each term
	int term_i = 0;
	for (TermIndex *pTermIndex = hTermIndex; pTermIndex != NULL; pTermIndex = pTermIndex->next) {
		int termIndex = pTermIndex->termIndex;
		float magnitude = coefVec[term_i++];
		csMatrix->countOccurrences(csMatrix->getCol(termIndex), occurrence, 0, magnitude);
	}
}

bool Model::isDuplicate(Model *model) {
	// check if all terms match
	TermIndex *p1TermIndex = hTermIndex;
	TermIndex *p2TermIndex = model->hTermIndex;
	
	while (!(p1TermIndex == NULL && p2TermIndex == NULL)) {
		if (p1TermIndex->termIndex != p2TermIndex->termIndex) return false;
		
		p1TermIndex = p1TermIndex->next;
		p2TermIndex = p2TermIndex->next;
	}
	
	return true;
}

Model::~Model() {
	
	// delete term index list
	while (hTermIndex != NULL) {
		TermIndex *removed = hTermIndex;
		hTermIndex = hTermIndex->next;
		delete removed;
	}
	
	// delete coefficients vector
	delete[] coefVec;
	
	// delete residuals vector
	delete[] resiVec;
	
	// delete model response
	delete[] modelResponse;
	
}
