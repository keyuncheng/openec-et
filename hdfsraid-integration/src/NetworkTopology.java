/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.apache.hadoop.net;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.LinkedList;
import java.util.Map;
import java.util.HashMap;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.util.StringUtils;

/** The class represents a cluster of computer with a tree hierarchical
 * network topology.
 * For example, a cluster may be consists of many data centers filled 
 * with racks of computers.
 * In a network topology, leaves represent data nodes (computers) and inner
 * nodes represent switches/routers that manage traffic in/out of data centers
 * or racks.  
 * 
 */
public class NetworkTopology {
  public final static String DEFAULT_RACK = "/default-rack";
  public final static int DEFAULT_HOST_LEVEL = 2;
  public static final Log LOG = 
    LogFactory.getLog(NetworkTopology.class);

  /* Inner Node represent a switch/router of a data center or rack.
   * Different from a leave node, it has non-null children.
   */
  private class InnerNode extends NodeBase {
    private ArrayList<Node> children=new ArrayList<Node>();
    private int numOfLeaves;
        
    /** Construct an InnerNode from a path-like string */
    InnerNode(String path) {
      super(path);
    }
        
    /** Construct an InnerNode from its name and its network location */
    InnerNode(String name, String location) {
      super(name, location);
    }
        
    /** Construct an InnerNode
     * from its name, its network location, its parent, and its level */
    InnerNode(String name, String location, InnerNode parent, int level) {
      super(name, location, parent, level);
    }
        
    /** Get its children */
    Collection<Node> getChildren() {return children;}
        
    /** Return the number of children this node has */
    int getNumOfChildren() {
      return children.size();
    }
        
    /** Judge if this node represents a rack 
     * Return true if it has no child or its children are not InnerNodes
     */ 
    boolean isRack() {
      if (children.isEmpty()) {
        return true;
      }
            
      Node firstChild = children.get(0);
      if (firstChild instanceof InnerNode) {
        return false;
      }
            
      return true;
    }
        
    /** Judge if this node is an ancestor of node <i>n</i>
     * 
     * @param n a node
     * @return true if this node is an ancestor of <i>n</i>
     */
    boolean isAncestor(Node n) {
      return getPath(this).equals(NodeBase.PATH_SEPARATOR_STR) ||
        (n.getNetworkLocation()+NodeBase.PATH_SEPARATOR_STR).
        startsWith(getPath(this)+NodeBase.PATH_SEPARATOR_STR);
    }
        
    /** Judge if this node is the parent of node <i>n</i>
     * 
     * @param n a node
     * @return true if this node is the parent of <i>n</i>
     */
    boolean isParent(Node n) {
      return n.getNetworkLocation().equals(getPath(this));
    }
        
    /* Return a child name of this node who is an ancestor of node <i>n</i> */
    private String getNextAncestorName(Node n) {
      if (!isAncestor(n)) {
        throw new IllegalArgumentException(
                                           this + "is not an ancestor of " + n);
      }
      String name = n.getNetworkLocation().substring(getPath(this).length());
      if (name.charAt(0) == PATH_SEPARATOR) {
        name = name.substring(1);
      }
      int index=name.indexOf(PATH_SEPARATOR);
      if (index !=-1)
        name = name.substring(0, index);
      return name;
    }

    /** Add node <i>n</i> to the subtree of this node 
     * @param n node to be added
     * @return true if the node is added; false otherwise
     */
    boolean add(Node n) {
      if (!isAncestor(n))
        throw new IllegalArgumentException(n.getName()+", which is located at "
                +n.getNetworkLocation()+", is not a decendent of "
                +getPath(this));
      if (isParent(n)) {
        // this node is the parent of n; add n directly
        n.setParent(this);
        n.setLevel(this.level+1);
        for(int i=0; i<children.size(); i++) {
          if (children.get(i).getName().equals(n.getName())) {
            children.set(i, n);
            return false;
          }
        }
        children.add(n);
        numOfLeaves++;
        return true;
      } else {
        // find the next ancestor node
        String parentName = getNextAncestorName(n);
        InnerNode parentNode = null;
        for(int i=0; i<children.size(); i++) {
          if (children.get(i).getName().equals(parentName)) {
            parentNode = (InnerNode)children.get(i);
            break;
          }
        }
        if (parentNode == null) {
          // create a new InnerNode
          parentNode = new InnerNode(parentName, getPath(this),
                                     this, this.getLevel()+1);
          children.add(parentNode);
        }
        // add n to the subtree of the next ancestor node
        if (parentNode.add(n)) {
          numOfLeaves++;
          return true;
        } else {
          return false;
        }
      }
    }
        
    /** Remove node <i>n</i> from the subtree of this node
     * @param n node to be deleted 
     * @return true if the node is deleted; false otherwise
     */
    boolean remove(Node n) {
      String parent = n.getNetworkLocation();
      String currentPath = getPath(this);
      if (!isAncestor(n))
        throw new IllegalArgumentException(n.getName()
                                           +", which is located at "
                                           +parent+", is not a descendent of "+currentPath);
      if (isParent(n)) {
        // this node is the parent of n; remove n directly
        for(int i=0; i<children.size(); i++) {
          if (children.get(i).getName().equals(n.getName())) {
            children.remove(i);
            numOfLeaves--;
            n.setParent(null);
            return true;
          }
        }
        return false;
      } else {
        // find the next ancestor node: the parent node
        String parentName = getNextAncestorName(n);
        InnerNode parentNode = null;
        int i;
        for(i=0; i<children.size(); i++) {
          if (children.get(i).getName().equals(parentName)) {
            parentNode = (InnerNode)children.get(i);
            break;
          }
        }
        if (parentNode==null) {
          return false;
        }
        // remove n from the parent node
        boolean isRemoved = parentNode.remove(n);
        // if the parent node has no children, remove the parent node too
        if (isRemoved) {
          if (parentNode.getNumOfChildren() == 0) {
            children.remove(i);
          }
          numOfLeaves--;
        }
        return isRemoved;
      }
    } // end of remove
        
    /** Given a node's string representation, return a reference to the node */ 
    private Node getLoc(String loc) {
      if (loc == null || loc.length() == 0) return this;
            
      String[] path = StringUtils.split(loc, PATH_SEPARATOR, 2);
      Node childnode = null;
      for(int i=0; i<children.size(); i++) {
        if (children.get(i).getName().equals(path[0])) {
          childnode = children.get(i);
        }
      }
      if (childnode == null) return null; // non-existing node
      if (path.length == 1) return childnode;
      if (childnode instanceof InnerNode) {
        return ((InnerNode)childnode).getLoc(path[1]);
      } else {
        return null;
      }
    }
        
    /** get <i>leafIndex</i> leaf of this subtree 
     * if it is not in the <i>excludedNode</i>*/
    private Node getLeaf(int leafIndex, Node excludedNode) {
      int count=0;
      // check if the excluded node a leaf
      boolean isLeaf =
        excludedNode == null || !(excludedNode instanceof InnerNode);
      // calculate the total number of excluded leaf nodes
      int numOfExcludedLeaves =
        isLeaf ? 1 : ((InnerNode)excludedNode).getNumOfLeaves();
      if (isRack()) { // children are leaves
        if (isLeaf) { // excluded node is a leaf node
          int excludedIndex = children.indexOf(excludedNode);
          if (excludedIndex != -1 && leafIndex >= 0) {
            // excluded node is one of the children so adjust the leaf index
            leafIndex = leafIndex>=excludedIndex ? leafIndex+1 : leafIndex;
          }
        }
        // range check
        if (leafIndex<0 || leafIndex>=this.getNumOfChildren()) {
          return null;
        }
        return children.get(leafIndex);
      } else {
        for(int i=0; i<children.size(); i++) {
          InnerNode child = (InnerNode)children.get(i);
          if (excludedNode == null || excludedNode != child) {
            // not the excludedNode
            int numOfLeaves = child.getNumOfLeaves();
            if (excludedNode != null && child.isAncestor(excludedNode)) {
              numOfLeaves -= numOfExcludedLeaves;
            }
            if (count+numOfLeaves > leafIndex) {
              // the leaf is in the child subtree
              return child.getLeaf(leafIndex-count, excludedNode);
            } else {
              // go to the next child
              count = count+numOfLeaves;
            }
          } else { // it is the excluededNode
            // skip it and set the excludedNode to be null
            excludedNode = null;
          }
        }
        return null;
      }
    }
        
    int getNumOfLeaves() {
      return numOfLeaves;
    }
  } // end of InnerNode
    
  InnerNode clusterMap = new InnerNode(InnerNode.ROOT); // the root
  // we store list instead of a set, for in-order, by-index access
  private final List<String> racks = new ArrayList<String>();
  private ReadWriteLock netlock;
  private Set<String> masterRacksSet = new HashSet<String>();

  // Xiaolu integrate for OEC
  public Map<String, Node> ipMap = new HashMap<String, Node>();
  public List<Node> oecIpList = new LinkedList<Node>();

  public Node getLocByIp(String ip) {
    return this.ipMap.get(ip);
  }

  public List<String> getIpCandidates(List<String> exclude) {
    List<String> toret = new LinkedList<String>();
    for (int i=0; i<oecIpList.size(); i++) {
      Node curnode = oecIpList.get(i);
      String nodename = curnode.getName();
      int idx = nodename.indexOf(":");
      String ip;
      if (idx > 0) {
        ip = nodename.substring(0, idx);
        if (exclude.contains(ip)) continue;
        toret.add(ip);
      }
    }
    return toret;
  }

  public Node getLocRandom() {
    Random randomGenerator = new Random();
    int randomidx = randomGenerator.nextInt(this.oecIpList.size());
    return oecIpList.get(randomidx);
  }
    
  public NetworkTopology() {
    netlock = new ReentrantReadWriteLock();
  }

  public NetworkTopology(Configuration conf) {
    this();
    String[] masterRacks = conf.getStrings("dfs.master.racks");
    if (masterRacks != null) {
      for (String masterRack : masterRacks) {
        masterRacksSet.add(masterRack);
      }
    }

  }
  
  /**
   * Get a copy of the racks list.
   */
  public List<String> getRacks() {
    netlock.readLock().lock();
    try {
      return new ArrayList<String>(racks);
    } finally {
      netlock.readLock().unlock();
    }
  }
    
  /** Add a leaf node
   * Update node counter & rack counter if neccessary
   * @param node
   *          node to be added
   * @exception IllegalArgumentException if add a node to a leave 
                                         or node to be added is not a leaf
   */
  public void add(Node node) {
    String loc = node.getName();
    int idx = loc.indexOf(":");
    if (idx > 0) {
      String ip = loc.substring(0, idx);
      if (!this.ipMap.containsKey(loc)) this.ipMap.put(ip, node); 
      if (!this.oecIpList.contains(node)) this.oecIpList.add(node);
    }

    if (node==null) return;
    if( node instanceof InnerNode ) {
      throw new IllegalArgumentException(
        "Not allow to add an inner node: "+NodeBase.getPath(node));
    }
    netlock.writeLock().lock();
    try {
      Node rack = getNode(node.getNetworkLocation());
      if (rack != null && !(rack instanceof InnerNode)) {
        throw new IllegalArgumentException("Unexpected data node " 
                                           + node.toString() 
                                           + " at an illegal network location");
      }
      if (clusterMap.add(node)) {
        LOG.info("Adding a new node: "+NodeBase.getPath(node));
        if (rack == null) {
          String rackName = node.getNetworkLocation();
          if (!racks.contains(rackName)) {
            racks.add(rackName);
          } else {
            LOG.error("Discrepancy between network topology and list of racks. "
                + "New rack was already in the list of racks: " + rackName);
          }
        }
      }
      LOG.debug("NetworkTopology became:\n" + this.toString());
    } finally {
      netlock.writeLock().unlock();
    }
  }
    
  /** Remove a node
   * Update node counter & rack counter if neccessary
   * @param node
   *          node to be removed
   */ 
  public void remove(Node node) {
    if (node==null) return;
    if( node instanceof InnerNode ) {
      throw new IllegalArgumentException(
        "Not allow to remove an inner node: "+NodeBase.getPath(node));
    }
    LOG.info("Removing a node: "+NodeBase.getPath(node));
    netlock.writeLock().lock();
    try {
      String currentRackName = node.getNetworkLocation();
      if (clusterMap.remove(node)) {
        InnerNode rack = (InnerNode)getNode(node.getNetworkLocation());
        if (rack == null) {
          if (!racks.remove(currentRackName)) {
            LOG.error("Discrepancy between network topology and list of racks. "
                + "Removed rack "
                + currentRackName
                + " was not in the rack list.");
          }
        }
      }
      LOG.debug("NetworkTopology became:\n" + this.toString());
    } finally {
      netlock.writeLock().unlock();
    }
  }
       
  /** Check if the tree contains node <i>node</i>
   * 
   * @param node
   *          a node
   * @return true if <i>node</i> is already in the tree; false otherwise
   */
  public boolean contains(Node node) {
    if (node == null) return false;
    netlock.readLock().lock();
    try {
      Node parent = node.getParent();
      for(int level=node.getLevel(); parent!=null&&level>0;
          parent=parent.getParent(), level--) {
        if (parent == clusterMap)
          return true;
      }
    } finally {
      netlock.readLock().unlock();
    }
    return false; 
  }
    
  /** Given a string representation of a node, return its reference
   * 
   * @param loc
   *          a path-like string representation of a node
   * @return a reference to the node; null if the node is not in the tree
   */
  public Node getNode(String loc) {
    netlock.readLock().lock();
    try {
      loc = NodeBase.normalize(loc);
      if (!NodeBase.ROOT.equals(loc))
        loc = loc.substring(1);
      return clusterMap.getLoc(loc);
    } finally {
      netlock.readLock().unlock();
    }
  }

  /**
   * Given a string representation of a rack, return its children
   * @param loc a path-like string representation of a rack
   * @return a newly allocated list with all the node's children
   */
  public List<Node> getDatanodesInRack(String loc) {
    netlock.readLock().lock();
    try {
      loc = NodeBase.normalize(loc);
      if (!NodeBase.ROOT.equals(loc))
        loc = loc.substring(1);
      InnerNode rack = (InnerNode) clusterMap.getLoc(loc);
      if (rack == null) return null;
      return new ArrayList<Node>(rack.getChildren());
    } finally {
      netlock.readLock().unlock();
    }
  }

  /** Return the total number of racks */
  public int getNumOfRacks() {
    netlock.readLock().lock();
    try {
      return racks.size();
    } finally {
      netlock.readLock().unlock();
    }
  }
 
  /** Returns the set of racks */
  public Set<String> getAllRacks() {
    netlock.readLock().lock();
    try {
      Set<String> result = new HashSet<String>(this.racks);
      result.addAll(this.masterRacksSet);
      return result;
    } finally {
      netlock.readLock().unlock();
    }
  }

  /** Return the total number of nodes */
  public int getNumOfLeaves() {
    netlock.readLock().lock();
    try {
      return clusterMap.getNumOfLeaves();
    } finally {
      netlock.readLock().unlock();
    }
  }
    
  /** Return the distance between two nodes
   * It is assumed that the distance from one node to its parent is 1
   * The distance between two nodes is calculated by summing up their distances
   * to their closest common  ancestor.
   * @param node1 one node
   * @param node2 another node
   * @return the distance between node1 and node2
   * node1 or node2 do not belong to the cluster
   */
  public int getDistance(Node node1, Node node2) {
    if (node1 == node2) {
      return 0;
    }
    Node n1=node1, n2=node2;
    int dis = 0;
    netlock.readLock().lock();
    try {
      int level1=node1.getLevel(), level2=node2.getLevel();
      while(n1!=null && level1>level2) {
        n1 = n1.getParent();
        level1--;
        dis++;
      }
      while(n2!=null && level2>level1) {
        n2 = n2.getParent();
        level2--;
        dis++;
      }
      while(n1!=null && n2!=null && n1.getParent()!=n2.getParent()) {
        n1=n1.getParent();
        n2=n2.getParent();
        dis+=2;
      }
    } finally {
      netlock.readLock().unlock();
    }
    if (n1==null) {
      LOG.warn("The cluster does not contain node: "+NodeBase.getPath(node1));
      return Integer.MAX_VALUE;
    }
    if (n2==null) {
      LOG.warn("The cluster does not contain node: "+NodeBase.getPath(node2));
      return Integer.MAX_VALUE;
    }
    return dis+2;
  } 
    
  /** Check if two nodes are on the same rack
   * @param node1 one node
   * @param node2 another node
   * @return true if node1 and node2 are pm the same rack; false otherwise
   * @exception IllegalArgumentException when either node1 or node2 is null, or
   * node1 or node2 do not belong to the cluster
   */
  public boolean isOnSameRack( Node node1,  Node node2) {
    if (node1 == null || node2 == null) {
      return false;
    }
      
    netlock.readLock().lock();
    try {
      return node1.getParent()==node2.getParent();
    } finally {
      netlock.readLock().unlock();
    }
  }
    
  final private static Random r = new Random();
  /** randomly choose one node from <i>scope</i>
   * if scope starts with ~, choose one from the all nodes except for the
   * ones in <i>scope</i>; otherwise, choose one from <i>scope</i>
   * @param scope range of nodes from which a node will be choosen
   * @return the choosen node
   */
  public Node chooseRandom(String scope) {
    netlock.readLock().lock();
    try {
      if (scope.startsWith("~")) {
        return chooseRandom(NodeBase.ROOT, scope.substring(1));
      } else {
        return chooseRandom(scope, null);
      }
    } finally {
      netlock.readLock().unlock();
    }
  }
  
  /**
   * Choose a rack which is not in exlcudedRacks
   * @param excludedRacks excluded racks
   * @return a rack
   */
  public String chooseRack(Set<String> excludedRacks) {
    String chosenRack = null;
    HashSet<Integer> chosenIndexes = new HashSet<Integer>();

    netlock.readLock().lock();
    try {
      int totalRacks = getNumOfRacks();
      if (totalRacks - excludedRacks.size() <= 0)
        return null; // all racks are excluded

      while (true) {
        int rackIndex;
        do {
          rackIndex = r.nextInt(totalRacks);
        } while (chosenIndexes.contains(rackIndex));
        
        chosenIndexes.add(rackIndex);
        chosenRack = racks.get(rackIndex);
        
        // if no excluded nodes are specified or the chosen node is not excluded
        // return it
        if (excludedRacks == null || (!excludedRacks.contains(chosenRack))) {
          return chosenRack;
        }
        // for sanity check if we used all indexes
        if (chosenIndexes.size() == totalRacks) {
          return null;
        }
      }
    } finally {
      netlock.readLock().unlock();
    }
  }
  
  public Node chooseRandom(String scope, String excludedScope){
    if (excludedScope != null) {
      if (scope.startsWith(excludedScope)) {
        return null;
      }
      if (!excludedScope.startsWith(scope)) {
        excludedScope = null;
      }
    }
    Node node = getNode(scope);
    if (!(node instanceof InnerNode)) {
      return node;
    }
    InnerNode innerNode = (InnerNode)node;
    int numOfDatanodes = innerNode.getNumOfLeaves();
    if (excludedScope == null) {
      node = null;
    } else {
      node = getNode(excludedScope);
      if (!(node instanceof InnerNode)) {
        numOfDatanodes -= 1;
      } else {
        numOfDatanodes -= ((InnerNode)node).getNumOfLeaves();
      }
    }
    int leaveIndex = r.nextInt(numOfDatanodes);
    return innerNode.getLeaf(leaveIndex, node);
  }
       
  /** return the number of leaves in <i>scope</i> but not in <i>excludedNodes</i>
   * if scope starts with ~, return the number of nodes that are not
   * in <i>scope</i> and <i>excludedNodes</i>; 
   * @param scope a path string that may start with ~
   * @param excludedNodes a list of nodes
   * @return number of available nodes
   */
  public int countNumOfAvailableNodes(String scope,
                                      Collection<Node> excludedNodes) {
    boolean isExcluded=false;
    if (scope.startsWith("~")) {
      isExcluded=true;
      scope=scope.substring(1);
    }
    scope = NodeBase.normalize(scope);
    int count=0; // the number of nodes in both scope & excludedNodes
    netlock.readLock().lock();
    try {
      for(Node node:excludedNodes) {
        if ((NodeBase.getPath(node)+NodeBase.PATH_SEPARATOR_STR).
            startsWith(scope+NodeBase.PATH_SEPARATOR_STR)) {
          count++;
        }
      }
      Node n=getNode(scope);
      int scopeNodeCount=1;
      if (n instanceof InnerNode) {
        scopeNodeCount=((InnerNode)n).getNumOfLeaves();
      }
      if (isExcluded) {
        return clusterMap.getNumOfLeaves()-
          scopeNodeCount-excludedNodes.size()+count;
      } else {
        return scopeNodeCount-count;
      }
    } finally {
      netlock.readLock().unlock();
    }
  }
    
  /** convert a network tree to a string */
  public String toString() {
    // print the number of racks
    StringBuffer tree = new StringBuffer();
    tree.append("Number of racks: ");
    tree.append(racks.size());
    tree.append("\n");
    // print the number of leaves
    int numOfLeaves = getNumOfLeaves();
    tree.append("Expected number of leaves:");
    tree.append(numOfLeaves);
    tree.append("\n");
    // print nodes
    for(int i=0; i<numOfLeaves; i++) {
      tree.append(NodeBase.getPath(clusterMap.getLeaf(i, null)));
      tree.append("\n");
    }
    return tree.toString();
  }

  /* swap two array items */
  static private void swap(Node[] nodes, int i, int j) {
    Node tempNode;
    tempNode = nodes[j];
    nodes[j] = nodes[i];
    nodes[i] = tempNode;
    
  }
  
  /** Sort nodes array by their distances to <i>reader</i>
   * It linearly scans the array, if a local node is found, swap it with
   * the first element of the array.
   * If a local rack node is found, swap it with the first element following
   * the local node.
   * If neither local node or local rack node is found, put a random replica
   * location at postion 0.
   * It leaves the rest nodes untouched.
   */
  public void pseudoSortByDistance( Node reader, Node[] nodes ) {
    int tempIndex = 0;
    if (reader != null ) {
      int localRackNode = -1;
      //scan the array to find the local node & local rack node
      for(int i=0; i<nodes.length; i++) {
        if(tempIndex == 0 && reader == nodes[i]) { //local node
          //swap the local node and the node at position 0
          if( i != 0 ) {
            swap(nodes, tempIndex, i);
          }
          tempIndex=1;
          if(localRackNode != -1 ) {
            if(localRackNode == 0) {
              localRackNode = i;
            }
            break;
          }
        } else if(localRackNode == -1 && isOnSameRack(reader, nodes[i])) {
          //local rack
          localRackNode = i;
          if(tempIndex != 0 ) break;
        }
      }

      // swap the local rack node and the node at position tempIndex
      if(localRackNode != -1 && localRackNode != tempIndex ) {
        swap(nodes, tempIndex, localRackNode);
        tempIndex++;
      }
    }
    
    // put a random node at position 0 if it is not a local/local-rack node
    if(tempIndex == 0 && nodes.length != 0) {
      swap(nodes, 0, r.nextInt(nodes.length));
    }
    
    // See if non-rack local nodes are in special racks
    int start = tempIndex;
    int end = nodes.length - 1;
    while (start < end) {
      // Find the node that is not in the special rack at the end of the
      // list of the nodes
      while (start < end && 
          masterRacksSet.contains(nodes[end].getNetworkLocation())) {
        end--;
      }
      // Find the node that is in the special rack and that comes before
      // the non special racks in the list of nodes
      while (start < end &&
          !masterRacksSet.contains(nodes[start].getNetworkLocation())) {
        start++;
      }
      // if start < end then start points at the node that is in the special
      // rack and end points at the node that is not in the special rack
      if (start < end) {
        swap(nodes, start, end);
        end--;
        start++;
      }
    }
  }

  /**
   * Form a pipeline of nodes by finding a shortest path that starts from the
   * writer and traverses all <i>nodes</i>. This is basically a traveling
   * salesman problem. The array of nodes will be modified in-place.
   * @param writer the node starting the pipeline
   * @param nodes the nodes to be traversed, modified in-place
   */
  public void getPipeline(Node writer, Node[] nodes) {
    if (nodes.length == 0) {
      return;
    }

    netlock.readLock().lock();
    try {
      if (writer == null || !contains(writer)) {
        writer = nodes[0];
      }
      for (int index = 0; index < nodes.length; index++) {
        Node shortestNode = nodes[index];
        int shortestDistance = getDistance(writer, shortestNode);
        int shortestIndex = index;
        for (int i = index + 1; i < nodes.length; i++) {
          Node currentNode = nodes[i];
          int currentDistance = getDistance(writer, currentNode);
          if (shortestDistance > currentDistance) {
            shortestDistance = currentDistance;
            shortestNode = currentNode;
            shortestIndex = i;
          }
        }
        //switch position index & shortestIndex
        if (index != shortestIndex) {
          nodes[shortestIndex] = nodes[index];
          nodes[index] = shortestNode;
        }
        writer = shortestNode;
      }
    } finally {
      netlock.readLock().unlock();
    }
  }
}
