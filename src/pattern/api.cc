/*

This Source Code Form is copyright of Yorkshire, Inc.
Copyright © 2014 Yorkshire, Inc,
Guiyang, Guizhou, China

This Source Code Form is copyright of 51Degrees Mobile Experts Limited.
Copyright © 2014 51Degrees Mobile Experts Limited, 5 Charlotte Close,
Caversham, Reading, Berkshire, United Kingdom RG4 7BY

This Source Code Form is the subject of the following patent
applications, owned by 51Degrees Mobile Experts Limited of 5 Charlotte
Close, Caversham, Reading, Berkshire, United Kingdom RG4 7BY:
European Patent Application No. 13192291.6; and
United States Patent Application Nos. 14/085,223 and 14/085,301.

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0.

If a copy of the MPL was not distributed with this file, You can obtain
one at http://mozilla.org/MPL/2.0/.

This Source Code Form is “Incompatible With Secondary Licenses”, as
defined by the Mozilla Public License, v. 2.0.

*/

#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <node.h>
#include <v8.h>
#include <nan.h>
#include "api.h"

#define BUFFER_LENGTH 50000

#ifdef _MSC_VER
#define _INTPTR 0
#endif

using namespace v8;

PatternParser::PatternParser(char * filename, char * requiredProperties) {
  dataSet = (fiftyoneDegreesDataSet *) malloc(sizeof(fiftyoneDegreesDataSet));
  result = fiftyoneDegreesInitWithPropertyString(filename, dataSet, requiredProperties);
  workSet = fiftyoneDegreesCreateWorkset(dataSet);
}

PatternParser::~PatternParser() {
  if (dataSet)
    free(dataSet);
  if (workSet)
    fiftyoneDegreesFreeWorkset(workSet);
}

void PatternParser::Init(Handle<Object> target) {
  Nan::HandleScope scope;
  Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
  // TODO(Yorkie): will remove
  t->InstanceTemplate()->SetInternalFieldCount(1);
  Nan::SetPrototypeMethod(t, "parse", Parse);
  Nan::Set(target, 
    Nan::New<v8::String>("PatternParser").ToLocalChecked(),
    t->GetFunction());
}

NAN_METHOD(PatternParser::New) {
  Nan::HandleScope scope;
  char *filename;
  char *requiredProperties;

  // convert v8 objects to c/c++ types
  v8::String::Utf8Value v8_filename(info[0]->ToString());
  v8::String::Utf8Value v8_properties(info[1]->ToString());
  filename = *v8_filename;
  requiredProperties = *v8_properties;

  // create new instance of C++ class PatternParser
  PatternParser *parser = new PatternParser(filename, requiredProperties);
  parser->Wrap(info.This());

  // valid the database file content
  switch(parser->result) {
    case DATA_SET_INIT_STATUS_INSUFFICIENT_MEMORY:
      return Nan::ThrowError("Insufficient memory");
    case DATA_SET_INIT_STATUS_CORRUPT_DATA:
      return Nan::ThrowError("Device data file is corrupted");
    case DATA_SET_INIT_STATUS_INCORRECT_VERSION:
      return Nan::ThrowError("Device data file is not correct");
    case DATA_SET_INIT_STATUS_FILE_NOT_FOUND:
      return Nan::ThrowError("Device data file not found");
    default:
      info.GetReturnValue().Set(info.This());
  }
}

NAN_METHOD(PatternParser::Parse) {
  Nan::HandleScope scope;

  // convert v8 objects to c/c++ types
  PatternParser *parser = ObjectWrap::Unwrap<PatternParser>(info.This());
  Local<Object> result = Nan::New<Object>();
  v8::String::Utf8Value v8_input(info[0]->ToString());

  fiftyoneDegreesWorkset *ws = parser->workSet;
  int maxInputLength = (parser->dataSet->header.maxUserAgentLength + 1) * sizeof(char);
  if ((unsigned)strlen(*v8_input) > (unsigned)maxInputLength) {
    return Nan::ThrowError("Invalid useragent: too long");
  }

  // here we should initialize the ws->input by hand for avoiding
  // memory incropted.
  memset(ws->input, 0, maxInputLength);
  memcpy(ws->input, *v8_input, strlen(*v8_input));
  fiftyoneDegreesMatch(ws, ws->input);

  if (ws->profileCount > 0) {

    // here we fetch ID
    int32_t propertyIndex, valueIndex, profileIndex;
    int idSize = ws->profileCount * 5 + (ws->profileCount - 1) + 1;
    char *ids = (char*) malloc(idSize);
    char *pos = ids;
    for (profileIndex = 0; profileIndex < ws->profileCount; profileIndex++) {
      if (profileIndex < ws->profileCount - 1)
        pos += snprintf(pos, idSize, "%d-", (*(ws->profiles + profileIndex))->profileId);
      else
        pos += snprintf(pos, idSize, "%d", (*(ws->profiles + profileIndex))->profileId);
    }
    Nan::Set(result, 
      Nan::New<v8::String>("Id").ToLocalChecked(), 
      Nan::New<v8::String>(ids).ToLocalChecked());
    free(ids);

    // build JSON
    for (propertyIndex = 0; 
      propertyIndex < ws->dataSet->requiredPropertyCount; 
      propertyIndex++) {

      if (fiftyoneDegreesSetValues(ws, propertyIndex) <= 0)
        break;

      const char *key = fiftyoneDegreesGetPropertyName(ws->dataSet, 
        *(ws->dataSet->requiredProperties + propertyIndex));
      Local<v8::Value> keyVal = Nan::New<v8::String>(key).ToLocalChecked();

      if (ws->valuesCount == 1) {
        const char *val = fiftyoneDegreesGetValueName(ws->dataSet, *(ws->values));
        
        // convert string to boolean
        if (strcmp(val, "True") == 0)
          Nan::Set(result, keyVal, Nan::True());
        else if (strcmp(val, "False") == 0)
          Nan::Set(result, keyVal, Nan::False());
        else
          Nan::Set(result, keyVal, Nan::New<v8::String>(val).ToLocalChecked());
      } else {
        Nan::MaybeLocal<Array> vals = Nan::New<Array>(ws->valuesCount - 1);
        for (valueIndex = 0; valueIndex < ws->valuesCount; valueIndex++) {
          const char *val = fiftyoneDegreesGetValueName(ws->dataSet, *(ws->values + valueIndex));
          Nan::Set(vals.ToLocalChecked(), 
            valueIndex, 
            Nan::New<v8::String>(val).ToLocalChecked());
        }
        Nan::Set(result, keyVal, vals.ToLocalChecked());
      }
    }

    Local<Object> meta = Nan::New<Object>();
    Nan::Set(meta, 
      Nan::New<v8::String>("difference").ToLocalChecked(),
      Nan::New<v8::Integer>(ws->difference));
    Nan::Set(meta,
      Nan::New<v8::String>("method").ToLocalChecked(), 
      Nan::New<v8::Integer>(ws->method));
    Nan::Set(meta,
      Nan::New<v8::String>("rootNodesEvaluated").ToLocalChecked(), 
      Nan::New<v8::Integer>(ws->rootNodesEvaluated));
    Nan::Set(meta,
      Nan::New<v8::String>("nodesEvaluated").ToLocalChecked(), 
      Nan::New<v8::Integer>(ws->nodesEvaluated));
    Nan::Set(meta,
      Nan::New<v8::String>("stringsRead").ToLocalChecked(), 
      Nan::New<v8::Integer>(ws->stringsRead));
    Nan::Set(meta,
      Nan::New<v8::String>("signaturesRead").ToLocalChecked(), 
      Nan::New<v8::Integer>(ws->signaturesRead));
    Nan::Set(meta,
      Nan::New<v8::String>("signaturesCompared").ToLocalChecked(), 
      Nan::New<v8::Integer>(ws->signaturesCompared));
    Nan::Set(meta,
      Nan::New<v8::String>("closestSignatures").ToLocalChecked(), 
      Nan::New<v8::Integer>(ws->closestSignatures));
    Nan::Set(result,
      Nan::New<v8::String>("__meta__").ToLocalChecked(), 
      meta);
  } else {
    Nan::False();
  }

  info.GetReturnValue().Set(result);
}

NODE_MODULE(pattern, PatternParser::Init)
