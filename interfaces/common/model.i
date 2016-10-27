////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Project:  Embedded Machine Learning Library (EMLL)
//  File:     model.i (interfaces)
//  Authors:  Chuck Jacobs
//
////////////////////////////////////////////////////////////////////////////////////////////////////

%{
#define SWIG_FILE_WITH_INIT
#include "Node.h"
#include "Model.h"
#include "Port.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "PortElements.h"
#include "InputNode.h"
#include "OutputNode.h"
#include "LoadModel.h"
%}

%nodefaultctor emll::model::NodeIterator;
%nodefaultctor emll::model::Node;
%nodefaultctor emll::model::Port;
%nodefaultctor emll::model::OutputPortBase;
%nodefaultctor emll::model::OutputPort<double>;
%nodefaultctor emll::model::InputPortBase;

%ignore std::hash<emll::model::PortElementBase>;
%ignore std::hash<emll::model::PortRange>;
%ignore emll::model::Model::ComputeOutput;
%ignore emll::model::InputPort::operator[];

%include "Port.h"
%include "OutputPort.h"
%include "PortElements.h"
%include "InputPort.h"
%include "Node.h"
%include "Model.h"
%include "InputNode.h"
%include "OutputNode.h"

%template (DoubleOutputPort) emll::model::OutputPort<double>;
%template (BoolOutputPort) emll::model::OutputPort<bool>;
%template () emll::model::PortElements<double>;
%template () emll::model::PortElements<bool>;

#ifndef SWIGXML
%template (NodeVector) std::vector<emll::model::Node*>;
%template (ConstNodeVector) std::vector<const emll::model::Node*>;
%template (PortVector) std::vector<emll::model::Port*>;
%template (InputPortVector) std::vector<emll::model::InputPortBase*>;
%template (OutputPortVector) std::vector<emll::model::OutputPortBase*>;

%template (DoubleInputNode) emll::model::InputNode<double>;
%template (BoolInputNode) emll::model::InputNode<bool>;
%template (DoubleOutputNode) emll::model::OutputNode<double>;

%template (DoubleInputNodeVector) std::vector<const emll::model::InputNode<double>*>;
%template (DoubleOutputNodeVector) std::vector<const emll::model::OutputNode<double>*>;

#endif

%extend emll::model::Model 
{
    // get input nodes
    std::vector<const emll::model::InputNode<double>*> GetDoubleInputNodes() const
    {
        return $self->GetNodesByType<emll::model::InputNode<double>>();
    }

    // get output nodes
    std::vector<const emll::model::OutputNode<double>*> GetDoubleOutputNodes() const
    {
        return $self->GetNodesByType<emll::model::OutputNode<double>>();
    }

    // compute output
    std::vector<double> ComputeDoubleOutput(const OutputPort<double>& outputPort) const
    {
        return $self->ComputeOutput(outputPort);
    }
}

%inline %{

class ELL_NodeIterator {
public:
    ELL_NodeIterator() : _iterator() {}
    bool IsValid() { return _iterator.IsValid(); }
    void Next() { _iterator.Next(); }
    const emll::model::Node* Get() { return _iterator.Get(); }
#ifndef SWIG
    ELL_NodeIterator(emll::model::NodeIterator& other) { _iterator = other; }
#endif
private:
    emll::model::NodeIterator _iterator;
};

class ELL_Model {
public:
    ELL_Model() {}
    ELL_Model(const std::string& filename) : _model(emll::common::LoadModel(filename)) {}
    void Save(const std::string& filename)
    {
        emll::common::SaveModel(_model, filename);
    }
    size_t Size() { return _model.Size(); }
    std::vector<const emll::model::InputNode<double>*> GetDoubleInputNodes() const
    {
        return _model.GetNodesByType<emll::model::InputNode<double>>();
    }
    std::vector<const emll::model::OutputNode<double>*> GetDoubleOutputNodes() const
    {
        return _model.GetNodesByType<emll::model::OutputNode<double>>();
    }
    std::vector<double> ComputeDoubleOutput(const emll::model::OutputPort<double>& outputPort) const
    {
        return _model.ComputeOutput(outputPort);
    }
    ELL_NodeIterator GetNodeIterator()
    {
        emll::model::NodeIterator iter = _model.GetNodeIterator();
        return ELL_NodeIterator(iter);
    }
#ifndef SWIG
    ELL_Model(const emll::model::Model& other) { _model = other; }
#endif

private:
    emll::model::Model _model;
};

%}
