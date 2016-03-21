#ifndef _NODE_H
#define _NODE_H
/*
 * Copyright (c) 2005-2015, Brian K. Vogel
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the FreeBSD Project.
 *
 */

#include "Matrix.h"
#include <string>
#include <iostream>
#include <functional>
#include <map>
#include "Constants.h"
#include "Utilities.h"

namespace kumozu {

  /**
   * This is an abstract base class for a node in a computational graph.
   *
   * A Node is an entity in a computational graph that can have an arbitrary number of named input ports and output ports. Each input or
   * output port contains a reference to two Matrix objects: one Matrix for the forward pass and one Matrix for the backward pass. Specifically,
   * an input port is associated with a read-only Matrix (the input activations) that data is read from during the forward pass.
   * This is the means by which
   * external data makes its way inside the node during the forward pass. The same input port also contains a reference to a writeable
   * Matrix (the input error gradients) that data is written to during the backward pass. Likewise, an output port also contains a reference
   * to two Matrix objects: Data is written into an "output activations" Matrix during the forward pass and data is read from an
   * "output errors" Matrix during the backward pass.
   *
   * Usage:
   *
   * As an example, suppose we have a Node that contains two input ports "A" and "B" and two output ports "C" and "D."
   * During the forward pass, data is read from the "input_activaions" matrices of "A" and "B". The Node may perform some operations
   * on this data and then write the output values into the "output_forward" matrices of "C" and "D." During the backward pass, the
   * same thing happens in the reverse direction (think of reversing all of the edge directions in the DAG).
   * Data is first read from the "output_backwards" matrices of "C" and "D", the Node may perform some operations on this data, and then
   * the results are written into the "input_backwards" matrices of "A" and "B."
   *
   * Shared nodes:
   *
   * In some networks, such as recurrent networks, it may be desired to replicate a node or sub-graph across time, keeping 
   * parameters tied across all time slices. To build such a network, first define a node (for example, called "slice") that represents the sub-graph
   * corresponding to a single time slice. Then create as many copies of "slice" as desired, calling set_shared("slice") on each copy.
   * This will cause the parameters in each of the copies to refer to the parameters in "slice.". 
   *
   * Fan out:
   *
   * Many (most) nodes support output ports with a fan out greater than 1. This is posssible as long as all nodes in the network accumulate into
   * their "input backward" activations during the backpropagation step. That is, in order for output ports with multiple fan out to function 
   * correctly, all "input backward" matrices should be set to 0 during the final part of the forward() call, which already is implemented
   * in this class. However, it is also necessary for each sub-class of Node in the network to accumulate rather than overwrite during the
   * backpropagation call.
   *
   * For nodes that do not accumulate into their "input backward" matrices during the backpropagation call, an explicit SplitterNode may be
   * used to acheive the same effect as multiple fan out.
   *
   */
  class Node {

  public:

  Node(std::string name) :
    m_is_initialized {false},
      m_name {name},
        m_enable_bias {true},
          m_is_train {false},
            m_epsilon {1e-4f},
              m_pass_relative_error {2e-2f}, // Minimum relative error needed to pass Jacobian checker.
                m_is_shared {false},
                  m_W_ref {m_W},
                    m_W_grad_ref {m_W_grad},
		      m_bias_ref {m_bias},
			m_bias_grad_ref {m_bias_grad}
                    {

                    }

                    /**
                     * Compute the output activations as a function of input activations.
                     *
                     * If this is the first time calling this function, the network will be initialized using
                     * the extents of the supplied input activations. The network will also be reinitialized
                     * any time that the extents of the supplied input activations change (although it is
                     * uncommon and bad for performance for this to occur frequently at runtime).
                     *
                     * Before calling this function for the first time, be sure that inputs have already beeen added
                     * to this node and connected to the appropriate contained sub-graph. Otherwise, this function will exit
                     * with an error.
                     */
                    void forward();

                    /**
                     * Perform a full back-propagation pass through the network.
                     *
                     * It is assumed that forward() has already been called with the same
                     * input activations.
                     *
                     * This call reads the curent output error gradients and updates the input error gradients
                     * (i.e., input deltas) and also updates the parameter gradients
                     * (weights and bias) for layers that contain and use such parameters.
                     *
                     * Note: This function is not virtual becuase subclasses should have no need to override it.
                     * subclasses should instead override back_propagate_deltas() and back_propagate_paramater_gradients() since
                     * they will be called by this function.
                     *
                     */
                    void back_propagate();

                    /**
                     * Back-propagate errors to compute new values for input_backward.
                     *
                     * The convention is that this function should be called before back_propagate_paramater_gradients().
                     * This function can also be called without calling back_propagate_paramater_gradients() if it is
                     * only desired to back-propagate the error deltas from the network output to its inputs. However,
                     * it is normally not recommended to call this function directly since it will be called by
                     * back_propagate().
                     *
                     * The output error must have already been updated before
                     * calling this method. Otherwise, the error gradients will not be back-propagated
                     * correctly.
                     *
                     * The default implementation assumes that this is a composite node. A subclass that is not
                     * a composite node should override this function.
                     */
                    virtual void back_propagate_deltas();

                    /**
                     * Compute the gradients for the paramaters (i.e.,  weights and bias).
                     *
                     * The convention is that this function should be called after back_propagate_deltas(). However,
                     * it is normally not recommended to call this function directly since it will be called by
                     * back_propagate().
                     *
		     * The default implementation assumes that this is a composite node. 
                     * This function has a default implementation that simply calls the same function on the contained nodes
		     * in the reverse order in which they were added. The results are then copied into the the parameter 
		     * matrix members of this node.
		     *
                     * A subclass that is not a composite node and that uses parameters should override
                     * this function. The "m_W_grad_ref" and "m_bias_grad_ref" member variables are provided by this base
		     * class for subclasses to use for the purpose of storing the parameter gradients. 
		     * The usual convention for SGD-based networks is that the subclass should accumulate into
		     * the parameter gradient matrices rather than overwriting previous values. 
		     * The parameter gradients are
		     * set to zero during the forward() call, and so this distinction will only be relavent
		     * if the network contains nodes with shared parameters.
                     *
                     * The output error activations (that is, "output backward") must have already been updated before
                     * calling this method. Otherwise, the error gradients will not be back-propagated
                     * correctly.
                     *
                     */
                    virtual void back_propagate_paramater_gradients();

		    /**
		     * Set all parameter gradients to be zero-valued.
		     *
		     * This base class implementation simply calls the same function on all contained nodes and copies the corresponding
		     * values into the corresponding member variables of this node. If there are no contained nodes, the parameter
		     * gradients matrices of this node are directly set to 0.
		     *
		     * A subclass that is not a composite node should override this function if it uses parameters.
		     */
		    virtual void zero_parameter_gradients();

		    /**
		     * For each input port of this node, set the values in "input backward" to 0.
		     *
		     * If this node has contained nodes, this function will be called recursively on them.
		     *
		     * The default implementation of forward() calls this function so that when it is time to start the
		     * backwards data pass, all "input backward" activations will have already been set to 0. This behavior
		     * is typically desired because most subclasses will accumulate gradients into "input backward" during the backward
		     * pass.
		     */
		    virtual void zero_input_backward();


                    /**
                     * Return the name of this node.
                     *
                     * @return The name of this node.
                     */
                    std::string get_name() const {
                      return m_name;
                    }

                    /**
                     * If this node is not composite, make it a shared node, using the parameters of the supplied node.
                     * Otherwise, if this is a composite node, call set_shared() recursively on the contained nodes.
		     *
                     * It is assumed that this node has a graph structure and parameters that are
                     * compatible with the supplied node. That is, this node can be thought of as a copy
                     * of the supplied node. Calling this function on a non-composite node will cause the parameters of this
                     * node to refer to those of the supplied node, meaning that any changes either node
                     * makes to the parameters will be seen by all other nodes that are sharing the same
                     * parameters.
                     *
                     * This function is intended to be used in graphs such as recurrent networks where it is
                     * convinient for training purposes to replicate a node in time so that each instance
                     * corresponds to a different time slice, but the parameters are tied across all time
                     * slices.
		     *
		     * This function may be called at any time after all contained nodes have
		     *  been added (if any), but
		     *  before the first forward data pass.
		     *
		     * @param node The node to which the parameters of this node will refer.
                     */
                    void set_shared(Node& node) {
		      if (is_composite()) {
			// If this node contains other nodes, call set_shared() on them as well.
			for (int i = 0; i < get_contained_node_count(); ++i) {
			  get_node(i).set_shared(node.get_node(i));
			}
		      } else {
			m_W_ref = std::ref(node.get_weights());
			m_W_grad_ref = std::ref(node.get_weight_gradient());
			m_bias_ref = std::ref(node.get_bias());
			m_bias_grad_ref = std::ref(node.get_bias_gradient());
			m_is_shared = true;
		      }
                    }

                    /**
                     * Return true if this node is using shared parameters. Otherwise return false.
                     *
                     * By default, a node is not shared and so this function will return false. However,
                     * if set_shared() is called, this function will return true.
                     */
                    const bool is_shared() const {
                      return m_is_shared;
                    }

                    /**
                     * Create a new input port that is associated with the supplied matrices.
                     *
                     * The two supplied matrices will be associated with the newly-created input port name "input_name" of
                     * this node. The first of these matrices is "input_forward" which is read from during the
                     * forward data propagation. The second of these is "input_backward" in which this node will write
                     * the input error gradients during the backward propagation.
                     *
                     * The input port "input" will be created by this function. If an existing port with the same name already
                     * exists, it will be replaced and associated with the supplied matrices.
                     *
                     * In all cases, calling this function will cause is_initialized() to return false.
                     *
                     * This function may be called multiple times to connect different matrices to different
                     * inputs of this node.
                     *
                     * @param input_forward An input matrix to connect to an input of this node. This is a read-only matrix
                     * that contains the activations for the forward pass.
                     *
                     * @param input_backward An input errors matrix to connect to an input of this node. This is a matrix that this
                     * node will write to during the backward pass.
                     * @param input_name The name of a new input to this node that will be created by this function and connected to
                     * the supplied matrices.
                     */
                    void create_input_port(const MatrixF& input_forward, MatrixF& input_backward, std::string input_name);

                    /**
                     * Create a new input port for this node that is associated with the supplied matrices.
                     *
                     * The two supplied matrices will be associated with the newly-created input port with the default name.
                     * The first of these matrices is "input_forward" which is read from during the
                     * forward data propagation. The second of these is "input_backward" in which this node will write
                     * the input error gradients during the backward propagation.
                     *
                     * Calling this function will remove any existing input port with the default name and then create an input port with the
                     * default name that will be associated with the supplied matrices. This function should therefore only be used when
                     * it is desired for this node to have exactly 1 input port.
                     *
                     * In all cases, calling this function will cause is_initialized() to return false.
                     *
                     * @param input_forward An input matrix to connect to an input of this node. This is a read-only matrix
                     * that contains the activations for the forward pass.
                     *
                     * @param input_backward An input errors matrix to connect to an input of this node. This is a matrix that this
                     * node will write to during the backward pass.
                     */
                    void create_input_port(const MatrixF& input_forward, MatrixF& input_backward);

                    /**
                     * Connect an output port of a parent node to an input port of this node, creating the input port in the process.
                     *
                     * Before calling this function, the output port "parent_output" of Node "parent" must already exist. Otherwise,
                     * an error will occur. The input port "input_name" will be created by this function. If an input port of the same
                     * name already exists, it will be replaced with a port of the same name that will be associated with the supplied
                     * parent output port.
                     *
                     * In all cases, calling this function will cause is_initialized() to return false.
                     *
                     * This function may be called multiple times to connect various output ports of various parent nodes to different
                     * input ports of this node.
                     *
                     * @param parent A parent node of this node.
                     * @param parent_output The name of an output port of "parent."
                     * @param input_name The name of a new input port for this node that will be created by this function and connected to the
                     * parent's output port as specified.
                     */
                    void create_input_port(Node& parent, std::string parent_output, std::string input_name);

		    /**
                     * Connect an output port of a parent node to an input port of this node, creating the input port in the process.
                     *
		     * This function is intended to be used for the case where the supplied "parent" node only has 1 output node. In
		     * this case, there corresponding name of the parent's output port does not need to be specified. However, it is
		     * assumed that this node might have several input nodes and so the input port name "input_name" does need to
		     * be specified.
		     *
                     * Before calling this function, the output port of Node "parent" must already exist. Otherwise,
                     * an error will occur. The input port of this node with name "input_name" will be created by this function. If an input port of the same
                     * name already exists, it will be replaced with a port of the same name.
                     *
                     * In all cases, calling this function will cause is_initialized() to return false.
                     *
                     * This function may be called multiple times to connect various output ports of various parent nodes to different
                     * input ports of this node.
		     *
                     *
                     * @param parent A parent node of this node.
                     * @param input_name The name of a new input port for this node that will be created by this function and connected to the
                     * parent's output port as specified.
                     */
		    void create_input_port_this_name(Node& parent, std::string input_name);

		    /**
                     * Connect an output port of a parent node to an input port of this node, creating the input port in the process.
                     *
		     * This function is intended to be used for the case where the supplied "parent" node has potentially several output nodes
		     * and this node will have only 1 input port. In this case, only the parent's output port name needs to be specified; the
		     * name of the input port to be created for this node does not need to be specified since this function will create
		     * an input port with the default input port name.
In		     *
                     * Before calling this function, the output port of Node "parent" must already exist. Otherwise,
                     * an error will occur. If this node already contains an input port with the default name, it will be replaced by
		     * with a new input port of the same default name.
                     *
                     * In all cases, calling this function will cause is_initialized() to return false.
                     *
                     * @param parent A parent node of this node.
                     * @param parent_output The name of an output port of "parent."
                     */
		    void create_input_port_parent_name(Node& parent, std::string parent_output);

                    /**
                     * Connect the output of the specified parent node to this node, creating an input port
                     * with the default name in the process.
                     *
                     * This function is intended to be used when it is desired to connect a parent node that
                     * contains exactly 1 output port to this node, giving this node exactly 1 input port.
                     * Therefore, when this function is called, the specified parent node must have exactly 1 output port
                     * and any existing input port of this node with the default name will be removed and replaced with an input port with the
                     * default name.
                     * Since no port names are used by this function, the name of the parent node's output port does not matter.
                     *
                     * In all cases, calling this function will cause is_initialized() to return false.
                     *
                     * @param parent A parent node of this node.
                     */
                    void connect_parent(Node& parent);

                    /**
                     * Return the number of input ports.
                     *
                     * @return the number of input ports.
                     */
                    int get_input_port_count() const {
                      return static_cast<int>(m_input_port_forward_map.size());
                    }


                    /**
                     * Delete the input port of this Node with the supplied name.
                     *
                     * If no port already exists with the supplied name, do nothing.
                     *
                     * @param name Name of input port to delete.
                     */
                    void delete_input_port(std::string name);

                    /**
                     * Delete all input ports from this Node.
                     *
                     * If no ports already exist, do nothing.
                     */
                    void delete_all_input_ports();

                    /**
                     * Connect an input port of this node to an input of a contained node.
                     *
                     * The input port "input_name" of this node is connected to the input "contained_input" of the contained node "contained."
                     * This connection will be made just before the forward data pass, by the forward() function. At that time,
                     * this node must have the specified input port. Otherwise this function will exit with an error. If the contained
                     * node already has the specified input port, it will be deleted and a new input port with the same name will be
                     * created.
                     *
                     * @param input_name Name of input port of this node.
                     * @param contained Name of the contained node.
                     * @param contained_input Name of input port of the contained node.
                     */
                    void connect_input_to_contained_node(std::string input_name, Node& contained, std::string contained_input);

		    /**
                     * Connect an input port of this node to the input of a contained node that only uses 1 input port..
                     *
		     * This function may be used when this node has multiple input ports, one of which we wish to connect to a
		     * contained node that only uses 1 input port. 
                     * The input port "input_name" of this node is connected to the input port of the contained node "contained" and
		     * given the default input port name. Note that this function saves the user from having to specify a name for
		     * the input port of the contained node. Since the contained node will only use 1 input port, there is no
		     * ambiguity and the default port name can be used.
		     *
                     * This connection will be made just before the forward data pass, by the forward() function. At that time,
                     * this node must have the specified input port. Otherwise this function will exit with an error. If the contained
                     * node already has the specified input port, it will be deleted and a new input port with the same default name will be
                     * created.
                     *
                     * @param input_name Name of input port of this node.
                     * @param contained Name of the contained node.
                     */
                    void connect_input_to_contained_node(std::string input_name, Node& contained);

                    /**
                     * Connect the input port of this node to an input of a contained node.
                     *
                     * For the case where both this node and the contained node will have exactly 1 input port, this function
                     * may be used to avoid specifying the port names (the default names will be used).
                     *
                     * This connection will be made just before the forward data pass, by the forward() function. At that time,
                     * this node must have exactly 1 input port. Otherwise this function will exit with an error. If the contained
                     * node already has an input port, it will be deleted and a new input port with the default name will be
                     * created.
                     *
                     * @param contained Name of the contained node.
                     */
                    void connect_input_to_contained_node(Node& contained);

                    /**
                     * Create an output port for this Node that is associated with the two supplied matrices.
                     *
                     * @param output_forward The output activations (read-only) for the output port.
                     * @param output_backward The output error (writable) for the output port.
                     * @param output_name The name for the output port.
                     */
                    void create_output_port(const MatrixF& output_forward, MatrixF& output_backward, std::string output_name);

		    /**
                     * Create a new output port for this node that is connected to the output port of a contained node.
		     *
		     * @param contained A node that is contained inside this node.
		     * @param contained_output The name of an output port of node "contianed."
		     * @param output_name The name of an output port of this node that will be created and connected to
		     * "contained_output" of node "contained."
                     */
                    void create_output_port(Node& contained, std::string contained_output, std::string output_name);

		    /**
                     * Create a new output port for this node that is connected to the output port of a contained node.
		     *
		     * This function is intended to be used to connect a contained node with exactly 1 output port to one of
		     * the (typically multiple) output ports of this node.
		     * This function may be used for the case where the contained node has excatly 1 output port (with any name),
		     * so that the corresponding port name does not need to be specified. However, the name of the output port
		     * for this node does need to be specified. 
		     *
		     * @param contained A node that is contained inside this node.
		     * @param output_name The name of an output port of this node that will be created and connected to
		     * "contained_output" of node "contained."		     
                     */
                    void create_output_port_this_name(Node& contained, std::string output_name);

		    /**
                     * Create a new output port for this node that is connected to an output port of a contained node.
		     *
		     * This function is intended to be used to connect a contained node with possibly several output ports to the
		     * sole output port of this node.
		     * Since this node will only have 1 output port, the port name does not need to be specified. However, since the
		     * contained node may have several output ports, its port name does need to be specified.
		     *
		     * @param contained A node that is contained inside this node.
		     * @param output_name The name of the output port of the contained node that will be connected to a new output
		     * port of default name of this node.	     
                     */
                    void create_output_port_contained_name(Node& contained, std::string output_name);

		    /**
		     * Create a new output port for this node that is connected to the output port of the specified contained ndoe.
		     *
		     * Since this function takes no port names, it is intended to be used for the case where the contained node
		     * has exactly 1 output port (with any name) and also this node will have exactly 1 output port (created by
		     * this function call) which will be given the default output port name.
		     * This thus releives the user from having to specify port names when both nodes will only use 1 output port.
		     *
		     */
		    void create_output_port(Node& contained);

                    /**
                     * Return the number of output ports.
                     *
                     * @return the number of output ports.
                     */
                    int get_output_port_count() const {
                      return static_cast<int>(m_output_port_forward_map.size());
                    }

                    /**
                     * Return the output activations associated with the specified output port.
                     *
                     * @param name The name of the output port.
                     *
                     * @return A reference to the associated output activations.
                     */
                    const MatrixF& get_output_forward(std::string name) const;

                    /**
                     * Return the output activations associated with the output port.
                     *
                     * For a Node that has exactly 1 output port, this function may be
                     * used to obtain a reference to the output activations without
                     * needed to specify the port name.
                     *
                     * @return A reference to the associated output activations.
                     */
                    const MatrixF& get_output_forward() const;

                    /**
                     * Delete the output port of this Node with the supplied name.
                     *
                     * If no port already exists with the supplied name, exit with an error.
                     *
                     * @param output_name Name of output port to delete.
                     */
                    void delete_output_port(std::string name);

                    /**
                     * Delete all output ports from this Node.
                     *
                     * If no ports already exist, do nothing.
                     */
                    void delete_all_output_ports();

                    /**
                     * Return the output deltas/errors associated with the specified output port.
                     *
                     * @param name The name of the output port.
                     *
                     * @return A reference to the associated output deltas.
                     */
                    const MatrixF& get_output_backward(std::string name) const;

                    /**
                     * Return the output deltas/errors associated with the specified output port.
                     *
                     * @param name The name of the output port.
                     *
                     * @return A reference to the associated output deltas.
                     */
                    MatrixF& get_output_backward(std::string name);

                    /**
                     * Return the output deltas/errors associated with the single output port.
                     *
                     * This function can only be used for a node that has exactly 1 output port. Otherwise,
                     * this function will exit with an error.
                     *
                     * @return A reference to the associated output deltas.
                     */
                    const MatrixF& get_output_backward() const;

                    /**
                     * Return the output deltas/errors associated with the single output port.
                     *
                     * This function can only be used for a node that has exactly 1 output port. Otherwise,
                     * this function will exit with an error.
                     *
                     * @return A reference to the associated output deltas.
                     */
                    MatrixF& get_output_backward();

                    /**
                     * Return the output extents associated with the specified output port.
                     *
                     * Note that the output extents will not have meaningful values until the
                     * node has been initialized (i.e., forward() has been
                     * called at least once).
                     */
                    std::vector<int> get_output_extents(std::string name) const;


                    /**
                     * Return true if this node has already been initialized. Otherwise return false.
                     *
                     * If this node is currently uninitialized, calling forward() will initialize it.
                     */
                    bool is_initialized() const {
                      return m_is_initialized;
                    }

                    /**
                     * Set the mode of this layer to either train or test/evaluate.
                     *
                     * Some layers, such as dropout layers, behave differently between training
                     * and evaluation modes. Most other sub-layers can ignore this mode, however.
                     *
                     * The default value is false (that is, use evaluation mode be default).
                     */
                    void set_train_mode(bool is_train);

                    /**
                     * Return the current train/test mode of this node.
                     *
                     * @return True if currently in "train mode." False if currently in "test/evaluation mode."
                     */
                    bool is_train_mode() const {
                      return m_is_train;
                    }

                    /**
                     * Return a reference to the weights matrix.
                     *
                     * Note: The size will be zero until the node has been initialized.
		     * If this is a shared node, the return matrix will have size 0.
                     */
                    MatrixF& get_weights() {
		      return m_W_ref.get();
                    }

                    /**
                     * Return a reference to the weights matrix.
                     *
                     * Note: The size will be zero until the node has been initialized.
                     */
                    const MatrixF& get_weights() const {
                      return m_W_ref.get();
                    }

                    /**
                     * Return a reference to the bias vector.
                     *
                     * Note: The size will be zero until the node has been initialized.
                     */
                    MatrixF& get_bias() {
		      return m_bias_ref.get();
                    }

                    /**
                     * Return a reference to the bias vector.
                     *
                     * Note: The size will be zero until the node has been initialized.
                     */
                    const MatrixF& get_bias() const {
                      return m_bias_ref.get();
                    }


                    /**
                     * Return a reference to the weight gradient matrix.
                     *
                     * Note: The size will be zero until the node has been initialized.
                     */
                    MatrixF& get_weight_gradient() {
		      return m_W_grad_ref.get();
                    }

                    /**
                     * Return a reference to the weight gradient matrix.
                     *
                     * Note: The size will be zero until the node has been initialized.
                     */
                    const MatrixF& get_weight_gradient() const {
		      return m_W_grad_ref.get();
                    }

                    /**
                     * Return a reference to the bias gradient vector.
                     *
                     * Note: The size will be zero until the node has been initialized.
                     */
                    MatrixF& get_bias_gradient() {
                      return m_bias_grad_ref.get();
                    }

                    /**
                     * Return a reference to the bias gradient vector.
                     *
                     * Note: The size will be zero until the node has been initialized.
                     */
                    const MatrixF& get_bias_gradient() const {
                      return m_bias_grad_ref.get();
                    }

                    /**
                     * For modules that contain bias parameters, enable or disable the use of such parameters.
                     *
                     * By default, bias is enabled.
                     *
                     * is_enable: If true, bias will be enabled. If false, modules that contain bias parameters will
                     * ignore such parameters and/or use 0-valued parameters even if an outside class attempts to modify the
                     * bias values.
                     */
                    virtual void enable_bias(bool is_enable) {
                      m_enable_bias = is_enable;
                    }

                    /**
                     * Return whether bias is enabled for this node.
                     *
                     * @return True if bias is enabled. False if bias is not enabled.
                     */
                    bool is_enable_bias() {
                      return m_enable_bias;
                    }

                    /**
                     * Add a node to the list nodes contained by this node.
                     *
                     * All input ports to the supplied node must have been added before this
                     * function is called. Otherwise, an error will occur if an attempt is made to add
                     * another input port after this function is called.
                     *
                     * @param node A node that will be contained by this node.
                     */
                    void add_node(Node& node);

                    /**
                     * Return a reference to the n'th Node contained by this node.
                     */
                    Node& get_node(int n);

                    const Node& get_node(int n) const;

                    /*
                     * Return the number of nodes contained by this node, which is equal to the number of times
                     * add_node() has been called.
                     */
                    int get_contained_node_count() const;

		    /**
		     * Return true if this node is composite. Otherwise, return false.
		     *
		     * A composite node is defined as a node that contains internal nodes (i.e.,
		     * has contained nodes).
		     */
                    const bool is_composite() const;


                    /*
                     * This base implementation prints information for weights, bias, output activations,
                     * and corresponding gradients.
                     * Subclasses may override to provide additional or different information.
                     */
                    virtual void print_paramater_stats() const;

                    /*
                     * Save parameters to a file withe the prefix given
                     * by the supplied name.
                     *
                     * This base class function saves the weights and bias parameters only.
                     * Subclasses may override to save additional parameters if desired.
                     *
                     */
                    virtual void save_parameters(std::string name) const;

                    /*
                     * Load learned parameters from a file. The string name should be
                     * the same that was used to save the parameters when
                     * save_learning_info() was called.
                     *
                     * This base class function loads the weights and bias parameters only.
                     * Subclasses may override to load additional parameters if desired.
                     */
                    virtual void load_parameters(std::string name);


                    /**
                     * Check that the Jacobian computed using the finite differences method agrees with
                     * the Jacobian computed using the gradient back-propagation member functions.
                     *
		     * @param input_port_extents_map A map from input port name to dimensions (extents) of the matrices
		     * associated with that input port.
                     */
                    void check_jacobian_weights(std::map<std::string, std::vector<int>> input_port_extents_map); 

                    /**
                     * Check that the Jacobian computed using the finite differences method agrees with
                     * the Jacobian computed using the gradient back-propagation member functions.
                     *
		     * @param input_port_extents_map A map from input port name to dimensions (extents) of the matrices
		     * associated with that input port.
                     */
                    void check_jacobian_bias(std::map<std::string, std::vector<int>> input_port_extents_map);

                    /**
                     * Check that the Jacobian computed using the finite differences method agrees with
                     * the Jacobian computed using the gradient back-propagation member functions.
		     *
		     * @param input_port_extents_map A map from input port name to dimensions (extents) of the matrices
		     * associated with that input port.
                     */
                    void check_jacobian_input_backward(std::map<std::string, std::vector<int>> input_port_extents_map);


                    /**
                     * Check that the Jacobian computed using the finite differences method agrees with
                     * the Jacobian computed using the gradient back-propagation member functions.
                     *
                     * This is a convinience function that can be used when the Node has only 1 input port.
		     *
		     * @param input_extents The extents (dimensions) for the input port.
                     */
                    void check_jacobian_weights(std::vector<int> input_extents);

                    /**
                     * Check that the Jacobian computed using the finite differences method agrees with
                     * the Jacobian computed using the gradient back-propagation member functions.
                     *
                     * This is a convinience function that can be used when the Node has only 1 input port.
		     *
		     * @param input_extents The extents (dimensions) for the input port.
                     */
                    void check_jacobian_bias(std::vector<int> input_extents);

                    /**
                     * Check that the Jacobian computed using the finite differences method agrees with
                     * the Jacobian computed using the gradient back-propagation member functions.
                     *
                     * This is a convinience function that can be used when the Node has only 1 input port.
		     *
		     * @param input_extents The extents (dimensions) for the input port.
                     */
                    void check_jacobian_input_backward(std::vector<int> input_extents);




                    /**
                     * Return the input activations associated with the input port of this node
                     * with the specified name.
                     *
                     * @param name The name of the input port of this node.
                     *
                     * @return A reference to the associated input activations.
                     */
                    const MatrixF& get_input_port_forward(std::string name) const;

                    /**
                     * Return the input activations associated with the input port of this node.
                     *
                     * Provided that this Node has exactly 1 input port, this function will return
                     * the input activations without needing to specify a port name. That is, if a Node has
                     * only 1 input port, there is only 1 possible input activations Matrix to return and hence
                     * no need to supply a port name. However, if
                     * this Node does not have exactly 1 input port, exit with an error.
                     *
                     * @return A reference to the associated input activations.
                     */
                    const MatrixF& get_input_port_forward() const;

		    /**
		     * Apply the supplied function f(const MatrixF&) to each "input forward" matrix of each input port.
		     *
		     * The function f(const MatrixF&) must accept a single arugument of type "const MatrixF&". 
		     * It is suggested to use a lambda for f().
		     */
		    template <typename Func>
		      void for_each_input_port_forward(Func f) {
		      for (const auto& x : m_input_port_forward_map) {
			const MatrixF& cur_mat = x.second.get();
			f(cur_mat);
		      }
		    }

		    /**
		     * Apply the supplied function f(MatrixF&) to each "input backward" matrix of each input port.
		     *
		     * The function f(MatrixF&) must accept a single arugument of type "MatrixF&". 
		     * It is suggested to use a lambda for f().
		     */
		    template <typename Func>
		      void for_each_input_port_backward(Func f) {
		      for (const auto& x : m_input_port_backward_map) {
			MatrixF& cur_mat = x.second.get();
			f(cur_mat);
		      }
		    }

                    /**
                     * Return the input deltas (i.e., errors) associated with the input port of this node
                     * with the specified name.
                     *
                     * @param name The name of the input port of this node.
                     *
                     * @return A reference to the associated input errors.
                     */
                    const MatrixF& get_input_port_backward(std::string name) const;

                    /**
                     * Return the input deltas (i.e., errors) associated with the input port of this node
                     * with the specified name.
                     *
                     * @param name The name of the input port of this node.
                     *
                     * @return A reference to the associated input errors.
                     */
                    MatrixF& get_input_port_backward(std::string name);

                    /**
                     * Return the input deltas (i.e., errors) associated with the input port of this node.
                     *
                     * Provided that this Node has exactly 1 input port, this function will return
                     * the input errors (deltas) without needing to specify a port name. That is, if a Node has
                     * only 1 input port, there is only 1 possible input activations Matrix to return and hence
                     * no need to supply a port name. However, if
                     * this Node does not have exactly 1 input port, exit with an error.
                     *
                     * @return A reference to the associated input errors.
                     */
                    MatrixF& get_input_port_backward();

                    /**
                     * Return the input deltas (i.e., errors) associated with the input port of this node.
                     *
                     * Provided that this Node has exactly 1 input port, this function will return
                     * the input errors (deltas) without needing to specify a port name. That is, if a Node has
                     * only 1 input port, there is only 1 possible input activations Matrix to return and hence
                     * no need to supply a port name. However, if
                     * this Node does not have exactly 1 input port, exit with an error.
                     *
                     * @return A reference to the associated input errors.
                     */
                    const MatrixF& get_input_port_backward() const;

                    /**
                     * Compute the output activations as a function of input activations.
                     *
                     * The output activations that are updated by this function can be obtained by calling get_output_forward().
                     *
		     * If this is a coposite node, this function will call forward() on each of the contained nodes in the
		     * order that they were added to this node.
                     */
                    virtual void forward_propagate();

                    /**
                     * Reinitialize this node based on the current extents that are associated with the input ports.
                     *
                     * This function must be called before the node can be used and must also be called whenever the
                     * input extents change during runtime. Setting this node to an uninitialized state (that is, by
                     * calling set_initialized(false)) will also cause this function to be called just prior to the
                     * forward data pass.
                     * It is the responsibiity of the forward() function to perform
                     * these tasks.
                     *
                     *
                     * Note that a call to this function can be somewhat expensive since several matrices (such
                     * as the output activations and possibly the parameters as well) might need to
                     * be reallocated and initialized. As a general convention, parameter matrices should only
                     * be reallocated and/or reinitialized if their size changes.
                     *
                     * By default the weights (m_W), weight gradient (m_W_grad), bias (m_bias), bias gradient (m_bias_grad),
                     * output activations (m_output_forward), and output error (m_output_backward) will be initialized
                     * by this base class to have size 0. Thus, a subclass should use this function to realocate
                     * and initialize any of these matrices that is wishes to use.
                     */
                    virtual void reinitialize() {}

                    /**
                     * Set the initialization state of this Node.
                     *
                     * @param is_initialized True if initialized. False if not initialized.
                     */
                    void set_initialized(bool is_initialized) {
                      m_is_initialized = is_initialized;
                    }


  private:

                    /*
                     * Copy the elements of the supplied flat output backward matrix into the individual "output backward" matrices
                     * associated with each of the output ports.
                     *
                     * The size of the supplied matrix must be the same as the total number of elements of all output
                     * ports. Otherwise, exit with an error.
                     */
                    void copy_flat_output_backward_to_individual(const MatrixF& flat_output_backward);

                    /*
                     * Copy the elements of the individual "output forward" matrices into the supplied
                     * float output forward matrix.
                     *
                     * If the size of the supplied matrix is not equal to the total number of elements to be
                     * copied, it will be resized.
                     */
                    void copy_individual_to_flat_output_forward(MatrixF& flat_output_forward);


                    /*
                     * Copy weights from contained Layers into the m_W matrix of this container Layer.
                     */
                    void copy_weights_contained_layers_to_this();

                    /*
                     * This function cause the weights matrix for this composite layer to get copied
                     * into the corresponding matrices of the contained layers.
                     */
                    void copy_weights_this_to_contained_layers();

                    /*
                     * This function cause the weights gradient matrices for this contained layers to get copied
                     * into the corresponding matrix of this composite layer.
                     */
                    void copy_weights_gradients_contained_layers_to_this();

                    /*
                     * Copy bias from contained Layers into the m_bias matrix of this container Layer.
                     */
                    void copy_bias_contained_layers_to_this();

                    /*
                     * This function cause the m_bias matrix for this composite layer to get copied
                     * into the corresponding matrices of the contained layers.
                     */
                    void copy_bias_this_to_contained_layers();

                    /*
                     * This function cause the bias gradient matrices for this contained layers to get copied
                     * into the corresponding matrix of this composite layer.
                     */
                    void copy_bias_gradients_contained_layers_to_this();

                    /*
                     * Make connections from input ports of this node to various internal nodes.
                     *
                     * Be sure to wait until the expected input ports exist prior to calling.
                     */
                    void make_internal_input_port_connections();

                    bool m_is_initialized;

                    // Input port dictionaries.
                    // Maps from string name to Matrix reference.
                    std::map<std::string, std::reference_wrapper<const MatrixF>> m_input_port_forward_map; 
                    std::map<std::string, std::reference_wrapper<MatrixF>> m_input_port_backward_map;
                    // Store the most recent extents for each input port.
                    std::map<std::string, std::vector<int>> m_input_port_extents_map;
                    // Store the number of outgoing connections for each input port. This number shoud be 1 for each port. Otherwise,
                    // there is a connection error.
                    std::map<std::string, int> m_input_port_fan_out_map;

                    // Output port dictionaries.
                    std::map<std::string, std::reference_wrapper<const MatrixF>> m_output_port_forward_map;
                    std::map<std::string, std::reference_wrapper<MatrixF>> m_output_port_backward_map;
                    // Store the number of outgoing connections for each output port. This number shoud be 1 for each port. Otherwise,
                    // there is a connection error.
                    std::map<std::string, int> m_output_port_fan_out_map;

                    // Schedule list of nodes.
                    // Holds a reference to each node in contained by this node in the order in which they were added.
                    std::vector<std::reference_wrapper<Node>> m_contained_nodes;

                    // Simple class to wrap the triple of things that are needed in order to connect an input port of this node to the input
                    // port of a contained node.
                    class input_to_contained_info {
                    public:
                    input_to_contained_info(std::string input_name, Node& contained_node, std::string contained_input):
                      m_input_name {input_name},
                        m_contained_node {contained_node},
                          m_contained_input {contained_input}
                          {
                          }

                          std::string get_input_name() {
                            return m_input_name;
                          }

                          Node& get_contained_node() {
                            return m_contained_node;
                          }

                          const Node& get_contained_node() const {
                            return m_contained_node;
                          }

                          std::string get_contained_input() {
                            return m_contained_input;
                          }

                    private:
                          std::string m_input_name;
                          Node& m_contained_node;
                          std::string m_contained_input;
                    };

                    // Represents a list of connections to make from an input port of this node to the input port of
                    // a contained node.
                    std::vector<input_to_contained_info> m_input_to_internal_connections;

                    std::string m_name;
                    bool m_enable_bias;
                    bool m_is_train;

                    const float m_epsilon;
                    const float m_pass_relative_error;

                    bool m_is_shared;

                    // Default parameter weights matrix.
                    MatrixF m_W;
                    // Refers to m_W by default, but will refer to another node's m_W if set_shared() is called.
                    std::reference_wrapper<MatrixF> m_W_ref;

                    // Default gradient of parameter weights matrix.
                    MatrixF m_W_grad;
                    // Refers to m_W_grad by default, but will refer to another node's m_W_grad if set_shared() is called.
                    std::reference_wrapper<MatrixF> m_W_grad_ref;

		    // Default bias parameters.
		    MatrixF m_bias; 
		    // Refers to m_bias by default, but will refer to another node's m_bias if set_shared() is called.
                    std::reference_wrapper<MatrixF> m_bias_ref;

		    // Default gradient of bias parameters.
                    MatrixF m_bias_grad; 
		    // Refers to m_bias_grad by default, but will refer to another node's m_bias_grad if set_shared() is called.
                    std::reference_wrapper<MatrixF> m_bias_grad_ref;

		    // Empty matrix to return as the parameters if this is a shared node.
		    MatrixF m_empty_mat;

  };


}

#endif /* _NODE_H */