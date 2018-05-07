#include <libsolidity/Node.h>

std::string const c_node_code =
R"E(
pragma solidity ^0.4.14;

contract Ownable {
    address public owner;

    modifier onlyOwner(address _addr) {
        require((_addr == owner) || (address(0) == owner));
        _;
    }

    event OwnershipTransferred(address previousOwner, address newOwne);
    function transferOwnership(address newOwner) public payable onlyOwner(msg.sender) {
        emit OwnershipTransferred(owner, newOwner);
        owner = newOwner;
    }
}

library SafeMath {
    function mul(uint256 a, uint256 b) internal pure returns (uint256) {
        if (a == 0) {
            return 0;
        }
        uint256 c = a * b;
        assert(c / a == b);
        return c;
    }

    function sub(uint256 a, uint256 b) internal pure returns (uint256) {
        assert(b <= a);
        return a - b;
    }

    function add(uint256 a, uint256 b) internal pure returns (uint256) {
        uint256 c = a + b;
        assert(c >= a);
        return c;
    }
}

contract BasicToken  is Ownable{
    using SafeMath for uint256;

    mapping(address => uint256) balances;
    mapping(address => uint256) locked;

    uint256 totalSupply_;

    function totalSupply() public view returns (uint256) {
        return totalSupply_;
    }

    event Transfer(address from, address to, uint256 value);
    function transfer(address _to, uint256 _value) public returns (bool) {
        require(_to != address(0));
        require(locked[msg.sender].add(_value) <= balances[msg.sender]);

        // SafeMath.sub will throw if there is not enough balance.
        balances[msg.sender] = balances[msg.sender].sub(_value);
        balances[_to] = balances[_to].add(_value);
        emit Transfer(msg.sender, _to, _value);
        return true;
    }

    function balanceOf(address _owner) public view returns (uint256 balance) {
        return balances[_owner];
    }

    function lockOf(address _owner) public view returns (uint256 balance) {
        return locked[_owner];
    }

    event Lock(address to, uint256 value);
    function lock(address _to, uint256 _value)  public payable onlyOwner(msg.sender)
    {
        require(locked[_to].add(_value) <= balances[_to]);

        locked[_to] = locked[_to].add(_value);
        emit Lock(_to, _value);
    }

    event Unlock(address to, uint256 value);
    function unlock(address _to, uint256 _value)  public payable onlyOwner(msg.sender)
    {
        require(_value <= locked[_to]);

        locked[_to] = locked[_to].sub(_value);
        emit Unlock(_to, _value);
    }

    event Withdraw(address sender, uint256 value);
    function withdraw(uint256 _value)  public payable
    {
        require(locked[msg.sender] <= balances[msg.sender].sub(_value));

        totalSupply_ = totalSupply_.sub(msg.value);
        balances[msg.sender] = balances[msg.sender].sub(_value);
        msg.sender.transfer(_value);
        emit Withdraw(msg.sender, _value);
    }

    event Recharge(address sender, uint256 value);
    function ()  public payable
    {
        totalSupply_ = totalSupply_.add(msg.value);
        balances[msg.sender] = balances[msg.sender].add(msg.value);
        emit Recharge(msg.sender, msg.value);
    }
}

contract Node is BasicToken{
    struct NodeInfo{
        string    property;
        address   addr;
        uint   value;
        bool    created;
    }

    mapping(string =>NodeInfo)  m_nodedata;
    string[]  m_nodeids;

    function char(byte b) internal pure returns (byte c) {
		if (b < 10) return byte(uint8(b) + 0x30);
		else return byte(uint8(b) + 0x57);
	}

	function toAsciiString(address x) internal pure returns (string) {
		bytes memory s = new bytes(40);
		for (uint i = 0; i < 20; i++) {
			byte b = byte(uint8(uint(x) / (2**(8*(19 - i)))));
			byte hi = byte(uint8(b) / 16);
			byte lo = byte(uint8(b) - 16 * uint8(hi));
			s[2*i] = char(hi);
			s[2*i+1] = char(lo);
		}

		return string(s);
	}

	function uint2hexstr(uint i) internal pure returns (string) {
        if (i == 0)
            return "0x0";

        uint j = i;
        uint length;
        while (j != 0) {
            length++;
            j = j >> 4;
        }

        uint mask = 15;
        bytes memory bstr = new bytes(length+2);
        uint k = length + 1;
        while (i != 0){
            uint curr = (i & mask);
            bstr[k--] = curr > 9 ? byte(87 + curr ) : byte(48 + curr); // 55 = 65 - 10
            i = i >> 4;
        }


        bstr[0] = byte(48);
        bstr[1] = byte(120);
        return string(bstr);
    }

	function strConcat(string _a, string _b, string _c) internal pure returns (string){
        bytes memory _ba = bytes(_a);
        bytes memory _bb = bytes(_b);
        bytes memory _bc = bytes(_c);
        string memory ret = new string(_ba.length + _bb.length + _bc.length);
        bytes memory bret = bytes(ret);
        uint k = 0;
        for (uint i = 0; i < _ba.length; i++)bret[k++] = _ba[i];
        for (i = 0; i < _bb.length; i++) bret[k++] = _bb[i];
        for (i = 0; i < _bc.length; i++) bret[k++] = _bc[i];
        return string(ret);
    }

	function strConcat(string _a, string _b) internal pure returns (string){
	    return strConcat(_a, _b, '');
    }

    function strEqual(string _a, string _b) internal pure returns (bool){
        bytes memory _ba = bytes(_a);
        bytes memory _bb = bytes(_b);
        if(_ba.length != _bb.length)
            return false;

        for (uint i = 0; i < _ba.length; i++) {
            if(_ba[i] != _bb[i])
                return false;
        }

        return true;
    }

    event registerNodeEvent(address addr, uint256 value, string _id, string _property);
    function registerNode(address _addr, uint256 _value, string _id, string _property) public payable onlyOwner(msg.sender){
        require(bytes(_id).length == 128);
        require(!m_nodedata[_id].created);

        lock(_addr, _value);

        m_nodeids.push(_id);

        m_nodedata[_id] = NodeInfo(_property, _addr, _value, true);
        emit registerNodeEvent(_addr, _value, _id, _property);
    }

    event unregisterNodeEvent(address addr, uint256 value, string _id);
    function unregisterNode(string _id) public payable onlyOwner(msg.sender){
        require(m_nodedata[_id].created);
        
        unlock(m_nodedata[_id].addr, m_nodedata[_id].value);

        uint i = 0;
        for(; i < m_nodeids.length; i++){
            if(strEqual(_id, m_nodeids[i]))
                break;
        }

        for(; i < m_nodeids.length-1; i++){
            m_nodeids[i] = m_nodeids[i+1];
        }

        m_nodeids.length--;
        m_nodedata[_id].created = false;
        emit unregisterNodeEvent(m_nodedata[_id].addr, m_nodedata[_id].value, _id);
    }

    function getNode(string _id) public constant returns(string){
        string memory json = "{";

        if(m_nodedata[_id].created){
            json = strConcat(json, "\"id\":\"", _id);
            json = strConcat(json, "\",\"account\":\"", toAsciiString(m_nodedata[_id].addr));
            json = strConcat(json, "\",\"value\":\"", uint2hexstr(m_nodedata[_id].value));
            json = strConcat(json, "\",\"property\":", m_nodedata[_id].property);
        }

        json = strConcat(json, "}");
        return string(json);
    }

    function getAddress(address _addr) public constant returns(string){
        string memory json = "[";
        bool isfirst = true;

        for(uint i = 0; i < m_nodeids.length; i++){
            if(m_nodedata[m_nodeids[i]].addr != _addr){
                continue;
            }
            
            if(isfirst){
                isfirst = false;
            }else{
                json = strConcat(json, ",");
            }
            
            string memory  str = getNode(m_nodeids[i]);
            json = strConcat(json, str);
        }

        json = strConcat(json, "]");
        return string(json);
    }

    function getAllNode() public constant returns(string){
        string memory json = "[";

        for(uint i = 0; i < m_nodeids.length; i++){
            string memory  str = getNode(m_nodeids[i]);
            json = strConcat(json, str);

            if(i+1 < m_nodeids.length){
                json = strConcat(json, ",");
            }
        }

        json = strConcat(json, "]");
        return string(json);
    }
}

)E";
