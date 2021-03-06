/***********************************************/
//     Copyright (c) 2010-2012, Kana Shimizu
//         All rights reserved.
//              test_libslidesort.cpp
/***********************************************/

#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <math.h>
#include "parallelslidesort.h"

//#define SIMPLE_METHOD
#define AFFINITY_METHOD
#if 0
#define DEBUG_UPDATE_ERROR 
#define DEBUG_DEGREE 
#endif

#define WEIGHT 0.9f
#define SCALE_WEIGHT
//#define NON_SCALE_WEIGHT

int *res_mscls;
int cnt_p;
int num_of_seq_of_all_input;

//gigi
// base type: ATCG
// used to be 5 types, but no gap in simulated data.
int baseToIndex(char base){
  if(base == 'A') return 0;
  else if (base == 'T') return 1;
  else if (base == 'C') return 2;
  else if (base == 'G') return 3;
  else if (base == '-') return 4;
  else return -1;
}
char indexToBase(int idx){
  char base[] = "ATCG-";
  return base[idx]; 
}

#define NUM_OF_BASE_TYPE 5 //ATCG-
#define BASE_COUNT_THRESHOLD 1

//for hash implementation
class BaseRecord{
  public:
    //fields
    static int TOTAL;
    int base_count[NUM_OF_BASE_TYPE]; //order:ATCG-
    int total_count; // total increment of base_count
    long seqid;
    int pos;
    char original_base;
    char suspected_base;
    int affinity_count;

    //constructor without initial count
    BaseRecord(long seqid, int pos, char original_base){
      for(int i=0; i<NUM_OF_BASE_TYPE; i++){
        base_count[i] = 0;
      }
      TOTAL++;
      total_count = 0;
      affinity_count = 0;
      this->seqid = seqid;
      this->pos = pos;
      this->original_base = original_base;
      this->suspected_base = original_base; // default suspected_base = original_base
    }


    //constructor
    BaseRecord(int baseIndex, long seqid, int pos, char original_base){
      for(int i=0; i<NUM_OF_BASE_TYPE; i++){
        base_count[i] = 0;
      }
      base_count[baseIndex]++;
      TOTAL++;
      total_count = 1;
      affinity_count = 0;
      this->seqid = seqid;
      this->pos = pos;
      this->original_base = original_base;
      this->suspected_base = original_base; // default suspected_base = original_base
    }

    //method
    void increment(int b){
      base_count[b]++;
      total_count++;
    }

    int getBaseCount(int baseIndex){
      return base_count[baseIndex];
    }

    void printAllBaseCount(){
      for(int i=0; i<NUM_OF_BASE_TYPE; i++){
        cout<<this->getBaseCount(i);
        if(i+1!=NUM_OF_BASE_TYPE)
          cout<<",";
      }
      cout<<endl;
    }

    void voteResultbyMax(){
      //cout<<"voting!! total_count:"<<total_count<<endl;
      int idxOfMax = 0;
      for(int i=0;i<NUM_OF_BASE_TYPE-1;i++){
        if (base_count[i] > base_count[idxOfMax]){
          idxOfMax = i;
        }
      }


      if(base_count[idxOfMax] > BASE_COUNT_THRESHOLD)
        suspected_base = indexToBase(idxOfMax);
    }

    void nomalizeCounts(){
      for(int i=0;i<NUM_OF_BASE_TYPE;i++){

      }
    }
    //only print when suspected_base is different from original one 
    void printResult(){
      //cout<<"printResult: "<<seqid<<"-"<<pos<<endl;
      if(suspected_base != original_base)
        cout<<seqid<<","<<pos<<","<<suspected_base<<","<<original_base<<endl;
    }
    void printResultWithBaseCount(){
      //cout<<"printResult: "<<seqid<<"-"<<pos<<endl;
      if(suspected_base != original_base){
        cout<<seqid<<","<<pos<<","<<suspected_base<<","<<original_base<<",";
        this->printAllBaseCount();
      }
    }

    void addWithWeight(BaseRecord otherBR, float weight){
      for(int i=0;i<NUM_OF_BASE_TYPE;i++){
          base_count[i] += otherBR.base_count[i] * weight;
      }

      total_count = 0;
      for(int i=0;i<NUM_OF_BASE_TYPE;i++){
        total_count += base_count[i];
      }
    }

    void addWithScaledWeight(BaseRecord otherBR, float weight){
      affinity_count++;
      double scaledWeight = pow(weight,affinity_count);
      for(int i=0;i<NUM_OF_BASE_TYPE;i++){
        int score = int(otherBR.base_count[i] * scaledWeight +0.5);
        if(score > 0)
          base_count[i] += score;
      }

      total_count = 0;
      for(int i=0;i<NUM_OF_BASE_TYPE;i++){
        total_count += base_count[i];
      }
    }
};
int BaseRecord::TOTAL = 0;

typedef map<string,BaseRecord> BaseRecordMap;
BaseRecordMap errorMap;


/*******
Similar pairs are obtained through callback function.
Callback function should be implemented as follows.

int FunctionName(const char*, const char*, TYPE_INDEX, TYPE_INDEX, char*, char*, double);

Each argument shows following information.

- Fasta header of 1st sequence
- Fasta header of 2nd sequence
- ID of 1st sequence
- ID of 2st sequence
- Alignment of 1st sequence
- Alignment of 2st sequence
- Distance of a pair
- Aligned size of a pair
*********/




// hash implementation

/**
Update error map when distinct base shows at the same pos of similar pair
  if key not exists -> create BaseRecord value
  else -> increment
**/
void updateErrorMap(TYPE_INDEX seqid, int pos, char pairedBase, char original_base){
  stringstream ss;
  map<string, BaseRecord>::iterator it;
  ss << seqid << "-" << pos;
  string key = ss.str();
  ss.str("");

  //check if key exists
  if(errorMap.find(key) == errorMap.end()){
    #ifdef DEBUG_UPDATE_ERROR
    cout<<"create new key:"<<seqid<<","<<pos<<","<<pairedBase<<","<<original_base<<endl;
    #endif
    BaseRecord value(baseToIndex(pairedBase), seqid, pos, original_base);
    errorMap.insert(pair<string,BaseRecord>(key,value)); 
  }
  else{
  //print current value according to key
    #ifdef DEBUG_UPDATE_ERROR
    cout<<"found existed key"<<endl;
    #endif
    it=errorMap.find(key);
    BaseRecord br = it->second;
    //br.printAllBaseCount();
    br.increment(baseToIndex(pairedBase));
    it->second = br;
  }
}

BaseRecordMap::iterator findBaseRecord(TYPE_INDEX seqid1, int pos1, char base1){
  stringstream ss;
  BaseRecordMap::iterator it1;
  ss << seqid1 << "-" << pos1;
  string key1 = ss.str();
  ss.str("");
  
  //check if key1 exists
  it1 =errorMap.find(key1);
  if(it1 == errorMap.end()){
    // create without initial count
    BaseRecord br1(seqid1, pos1, base1);
    it1 = errorMap.insert(it1, pair<string,BaseRecord>(key1,br1)); 
  }

  return it1;
}


void updateMapWithAffinity(TYPE_INDEX seqid1, TYPE_INDEX seqid2, int pos1, int pos2, char base1, char base2, double weight){
  stringstream ss;

  ss << seqid1 << "-" << pos1;
  string key1 = ss.str();
  ss.str("");
  ss << seqid2 << "-" << pos2;
  string key2 = ss.str();
  ss.str("");

  BaseRecordMap::iterator it1 = findBaseRecord(seqid1,pos1,base1);
  BaseRecordMap::iterator it2 = findBaseRecord(seqid2,pos2,base2);

  BaseRecord br1 = it1->second;
  BaseRecord br2 = it2->second;

  #ifdef NON_SCALE_WEIGHT 
  br1.addWithWeight(br2,weight);
  br1.increment(baseToIndex(base2));
  br2.addWithWeight(br1,weight);
  br2.increment(baseToIndex(base1));
  #endif


  #ifdef SCALE_WEIGHT 
  br1.addWithScaledWeight(br2,weight);
  br1.increment(baseToIndex(base2));
  br2.addWithScaledWeight(br1,weight);
  br2.increment(baseToIndex(base1));
  #endif


  it1->second = br1;
  it2->second = br2; 


}
int degree(const char* fasta_head1, const char* fasta_head2, TYPE_INDEX seqid1, TYPE_INDEX seqid2, char* aln1, char* aln2, double dist,int aln_size)
{
  res_mscls[seqid1]++;
  res_mscls[seqid2]++;

  /**
  //if dist=0, NULL is returned.
  cnt_p++;
  if(cnt_p%1000==0){
    cerr<<cnt_p<<" pairs have been found\n";
  }
  **/

/** 
  cout<<"----\n";
  cout<<fasta_head1<<" , "<<fasta_head2<<" , "<<dist<<"\n";
  cout<<aln1<<"\n"; 
  cout<<aln2<<"\n"; 
  cout<<"----\n";
**/ 
  int gapCount1 = 0, gapCount2 = 0;

  for (int i = 0; i<aln_size; i++){

    bool hasGap = false; 
    
    char base1 = aln1[i];
    char base2 = aln2[i];

    if(base1 == '-') {gapCount1++; hasGap=true;}
    if(base2 == '-') {gapCount2++; hasGap=true;}
    
    #ifdef AFFINITY_METHOD
    if(!hasGap){

      //updateMapWithAffinity(seqid1, i-gapCount1, base2, base1);
      //updateMapWithAffinity(seqid2, i-gapCount2, base1, base2);
   
      updateMapWithAffinity(seqid1, seqid2, i-gapCount1, i-gapCount2, base1, base2, WEIGHT);
    }
    #endif
    
    #ifdef SIMPLE_METHOD
    if((base1 != base2) && !hasGap){
      # ifdef DEBUG_DEGREE
      cout<<"----\n";
      cout<<fasta_head1<<" , "<<fasta_head2<<" , "<<dist<<"\n";
      cout<<aln1<<"\n"; 
      cout<<aln2<<"\n"; 
      cout<<"----\n";
      cout<<seqid1<<", i:"<<i<<", gapCount1:"<<gapCount1<<", base1:"<<base1<<endl;
      cout<<seqid2<<", i:"<<i<<", gapCount2:"<<gapCount2<<", base2:"<<base2<<endl;
      # endif
      updateErrorMap(seqid1, i-gapCount1, base2, base1);
      updateErrorMap(seqid2, i-gapCount2, base1, base2);

    }
    #endif
  }
  
  return(0);
}

// hash implementation ends

// run if (ml.co.isRevComp) 
int degree2(const char* fasta_head1, const char* fasta_head2, TYPE_INDEX seqid1, TYPE_INDEX seqid2, char* aln1, char* aln2, double dist,int aln_size)
{
  res_mscls[seqid1/2]++;
  res_mscls[seqid2/2]++;

  /**
  //if dist=0, NULL is returned.
  cnt_p++;
  if(cnt_p%1000==0){
    cerr<<cnt_p<<" pairs have been found\n";
  }
  **/


  cout<<"it is degree2 method";
  cout<<"----\n";
  cout<<fasta_head1<<" , "<<fasta_head2<<" , "<<seqid1<<" , "<<seqid2<<"\n";
  cout<<aln1<<"\n";
  cout<<aln2<<"\n";
  cout<<"----\n";
  return(0);
}

int main(int argc, char **argv)
{
 
/**	
	In default setting, slidesort output all pairs to STDOUT.
	I recommend to use "-l" option which disables outputting pairs, if you run slidesort on big data.
**/
  
 /**
 read file again 
  store quality score  
**/
  cnt_p=0;
  //multisort ml;  // create object
  parallelslidesort ml;  // create object

  ml.free_vals_automatic=false;
/**
  In default setting, original sequence and related information is released after exec().
  If you would like to keep these data on memory,
  
  set free_vals_automatic=false.
  
  The data is kept on memory while the object (in this case "ml") is alive.
  Please take care of memory size when you use this option.

  Those information are accessible by following methods.

  string showFastaHeader(TYPE_INDEX seqid); // show fasta header of a sequence of seqid
  string showSequence(TYPE_INDEX seqid); // show a sequence of seqid
  TYPE_LONG showNumOfPairs(); // show total number of pairs
  TYPE_INDEX showNumOfSeqs(); // show total number of sequences. If you run slidesort with default setting, sequences including unknown character (e.g. n, N, Z, X) are excluded. If using -v, number of sequence is doubled for including reverse complement sequences.
  static TYPE_INDEX showRevCompID(TYPE_INDEX seqid); // If you use -v option, this function shows ID of Reverse complement sequence of seqid

**/



  // if parameters are obtained from command line
  if(ml.getParam(argc, argv)<0) return (1);

  // -o ml.co.outputfile set to false
  ml.co.outputfile = false;


  //set callback function
  if (ml.co.isRevComp) {
    ml.setResGetFuncPtr(degree2);
  } else {
    ml.setResGetFuncPtr(degree);
  }


  /**
  If you would like to set parameters by the code, 
  please use following options.

  cmlOptions inco;  // An object for setting options.

  inco.distance = 3; // set distance
  inco.charType = DNA; //set char type
  inco.exclude_unknown_character=true; // exclude sequences with unknown character (e.g. n, N, Z, X) or not.
  inco.key_size = 0; // key size is automatically chose if sat 0.

  //set distance type
  inco.distanceType = EDIT_DISTANCE; // set EDIT_DISTANCE for edit distance, or HAMMING_DISTANCE for hamming distance
  // set input
  inco.inputFileName = "input.fst"; // set input filename

  // set output
  inco.outputfile=true; // set true if outputting file. set false if not.
  inco.outputFileName="-"; // set output file name. set "-" if outputting result in stdout.
  inco.outputaln = true; // set true if outputting alignment for each pair, set false if not.

  // set gap cost
  inco.gap_limit=-1;  // gap width
  inco.gap_cost = 1.0; // gap cost
  inco.gap_open = 0; // gap open cost

  //Options for comparing two datasets
  inco.isMapMode = false; //set true if comparing two datasets. set false if not.
  inco.fst_head = 0; // set position of a top sequence of 1st dataset. Positions of the 1st sequence, 2nd sequence, 3rd sequence, ... are referred to as 0, 1, 2, .. .  
  inco.fst_size = 0; //size of first dataset. if 0, automatic detect
  inco.snd_head = 100; // Set position of a top sequence of 2nd dataset.
  inco.snd_size = 0; //set size of second dataset. if 0, automatic detect.
  inco.exclude_unknown_character=true;

  //Options for searching on subset of an input sequences
  inco.isPartialMode=false; // set true if part of datasets are used. set false if not.
  inco.fst_head = 30; // set position of a top sequence.
  inco.fst_size = 0; // set size of dataset.

  //Options for reverse complement search
  inco.isRevComp=false;// set true if searching with reverse complement
  inco.isOutputBothStrand=false;// set true if outputting a same pair twice if both strand make similar pairs (dist(A,B)<=d and dist(A,B')<=d. B' is reverse complement of B).

  ml.getParam(inco);
  **/

  cmlOptions inco;  // An object for setting options.
  // set output
  inco.outputfile=true; // set true if outputting file. set false if not.
  inco.outputFileName="output.txt"; // set output file name. set "-" if outputting result in stdout.
  inco.outputaln = true; // set true if outputting alignment for each pair, set false if not.


  num_of_seq_of_all_input = ml.showNumOfSeqs();
  if (ml.co.isRevComp) {
     num_of_seq_of_all_input /= 2;
  }

  res_mscls = (int *)malloc(sizeof(TYPE_INDEX)*num_of_seq_of_all_input);
  memset(res_mscls,0x00,sizeof(TYPE_INDEX)*num_of_seq_of_all_input);
  if(ml.exec() < 0) return (1);

  cerr<<"weight = "<<WEIGHT<<endl;
  cerr<<"num of similar pairs = "<<ml.showNumOfPairs()<<"\n";
  cerr<<"num of sequences = "<<ml.showNumOfSeqs()<<"\n";

//comment out by gigi
/**
  if (ml.co.isRevComp) {
    for(int i=0;i<num_of_seq_of_all_input;i++){
      cerr<<ml.showFastaHeader(i*2)<<": "<<res_mscls[i]<<"\n";
    }
  } else {
    for(int i=0;i<num_of_seq_of_all_input;i++){
      cerr<<ml.showFastaHeader(i)<<": "<<res_mscls[i]<<"\n";
    }
  }
**/
/**

*** hash version ****
gigi after degree 
print errormap and output result file

**/

cerr<<"after degree"<<endl;

cerr<<"errormap size:"<<errorMap.size()<<endl;
cout<<"seq,pos,correct_base,error_base";
cout<<",A,T,C,G,-";
cout<<endl;
map<string, BaseRecord>::iterator it;
for(it=errorMap.begin(); it != errorMap.end(); it++){
  //cout <<it->first<<endl;
  BaseRecord br = it->second;

  br.voteResultbyMax();
  //br.printResult();
  br.printResultWithBaseCount();
}



  free(res_mscls);

  return (0);
}
