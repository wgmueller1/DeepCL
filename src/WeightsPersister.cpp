// Copyright Hugh Perkins 2015 hughperkins at gmail
//
// This Source Code Form is subject to the terms of the Mozilla Public License, 
// v. 2.0. If a copy of the MPL was not distributed with this file, You can 
// obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>
#include <cstring>

#include "FileHelper.h"
#include "NeuralNet.h"

#include "WeightsPersister.h"

using namespace std;

#undef VIRTUAL
#define VIRTUAL 
#undef STATIC
#define STATIC

template< typename T > STATIC void WeightsPersister::copyArray( T *dst, T const*src, int length ) { // this might already be in standard C++ library?
    memcpy( dst, src, length * sizeof(T) );
}
STATIC int WeightsPersister::getTotalNumWeights( NeuralNet *net ) {
    int totalWeightsSize = 0;
//    cout << "layers size " << net->layers.size() << endl;
    for( int layerIdx = 1; layerIdx < (int)net->layers.size(); layerIdx++ ) {
        Layer *layer = net->layers[layerIdx];
        int thisPersistSize = layer->getPersistSize();
//        cout << "layer " << layerIdx << " this persist size " << thisPersistSize << endl;
        totalWeightsSize += thisPersistSize;
    }
    return totalWeightsSize;
}
STATIC void WeightsPersister::copyNetWeightsToArray( NeuralNet *net, float *target ) {
    int pos = 0;
    for( int layerIdx = 1; layerIdx < (int)net->layers.size(); layerIdx++ ) {
        Layer *layer = net->layers[layerIdx];
        int persistSize = layer->getPersistSize();
        if( persistSize > 0 ) {
            layer->persistToArray( &(target[pos]) );
        }
        pos += persistSize;
    }
}
STATIC void WeightsPersister::copyArrayToNetWeights( float const*source, NeuralNet *net ) {
    int pos = 0;
    for( int layerIdx = 1; layerIdx < (int)net->layers.size(); layerIdx++ ) {
    Layer *layer = net->layers[layerIdx];
        int persistSize = layer->getPersistSize();
        if( persistSize > 0 ) {
            layer->unpersistFromArray( &(source[pos]) );
        }
        pos += persistSize;
    }
}
STATIC void WeightsPersister::persistWeights( std::string filepath, std::string trainingConfigString, NeuralNet *net, int epoch, int batch, float annealedLearningRate, int numRight, float loss ) {
    int headerLength = 1024;
    int totalWeightsSize = getTotalNumWeights( net );
    char *persistArray = new char[headerLength + totalWeightsSize * sizeof(float) ];
    int *persistArrayInts = reinterpret_cast<int *>(persistArray);
    float *persistArrayFloats = reinterpret_cast<float *>(persistArray);
    strcpy_safe( persistArray, "ClCn", 4 ); // so easy to recognise file type
    persistArrayInts[1] = 1; // data file version number
    persistArrayInts[2] = epoch;
    persistArrayInts[3] = batch;
    persistArrayInts[4] = numRight;
    persistArrayFloats[5] = loss;
    persistArrayFloats[6] = annealedLearningRate;
    strcpy_safe( persistArray + 7 * 4, trainingConfigString.c_str(), 800 );
    copyNetWeightsToArray( net, reinterpret_cast<float *>(persistArray + headerLength) );
    FileHelper::writeBinary( "~" + filepath, reinterpret_cast<char *>(persistArray), 
        headerLength + totalWeightsSize * sizeof(float) );
    FileHelper::remove( filepath );
    FileHelper::rename( "~" + filepath, filepath );
    std::cout << "wrote weights to file, filesize " << ( ( headerLength + totalWeightsSize ) *sizeof(float)/1024) << "KB" << std::endl;
    delete[] persistArray;
}
STATIC bool WeightsPersister::loadWeights( std::string filepath, std::string trainingConfigString, NeuralNet *net, int *p_epoch, int *p_batch, float *p_annealedLearningRate, int *p_numRight, float *p_loss ) {
    if( FileHelper::exists( filepath ) ){
        int headerSize = 1024;
        long fileSize;
        char * data = FileHelper::readBinary( filepath, &fileSize );
        data[headerSize - 1] = 0; // null-terminate the string, if not already done
        float *allWeightsArray = reinterpret_cast<float *>(data + headerSize);
        std::cout << "read weights from file "  << (fileSize/1024) << "KB" << std::endl;
        int expectedTotalWeightsSize = getTotalNumWeights( net );
        int numFloatsRead = ( fileSize - headerSize ) / sizeof( float );
        if( expectedTotalWeightsSize != numFloatsRead ) {
            throw std::runtime_error("weights file contains " + toString(numFloatsRead) + " floats, but we expect to see: " + toString( expectedTotalWeightsSize ) + ".  So there is probably some mismatch between the weights file, and the settings, or network version, used." );
        }
        int *dataAsInts = reinterpret_cast<int *>(data);
        float *dataAsFloats = reinterpret_cast<float *>(data);
        if( data[0] != 'C' || data[1] != 'l' || data[2] != 'C' || data[3] != 'n' ) {
            std::cout << "weights file not ClConvolve format" << std::endl;
            return false;
        }
        if( dataAsInts[1] != 1 ) {
            std::cout << "weights file version not known" << std::endl;
            return false;
        }
        if( trainingConfigString != std::string( data + 7 * 4 ) ) {
            std::cout << "training options dont match weights file" << std::endl;
            return false;
        }
        *p_epoch = dataAsInts[2];
        *p_batch = dataAsInts[3];
        *p_numRight = dataAsInts[4];
        *p_loss = dataAsFloats[5];
        *p_annealedLearningRate = dataAsFloats[6];
        copyArrayToNetWeights( allWeightsArray, net );
        delete [] data;
        return true;
    }
    return false;
}

