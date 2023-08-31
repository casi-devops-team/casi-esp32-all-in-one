const sidebar = document.getElementById("sidebar");

fetch("/sidebar.html").then(x => x.text()).then((response) => {
    sidebar.innerHTML = response;
    const sidebarList = document.querySelectorAll(".sidebar-item");
    sidebarList.forEach(sidebarItem => {
        if (sidebarItem.getAttribute("href") == window.location.pathname) {
            sidebarItem.classList.add("active");
        }
    });
});

function isValidMACAddress(str) {
    // Regex to check valid
    // MAC_Address 
    let regex = new RegExp(/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})|([0-9a-fA-F]{4}.[0-9a-fA-F]{4}.[0-9a-fA-F]{4})$/);
 
    // if str
    // is empty return false
    if (str == null) {
        return false;
    }
 
    // Return true if the str
    // matched the ReGex
    if (regex.test(str) == true) {
        return true;
    }
    else {
        return false;
    }
}

function validateMacList() {
    let valid = true;
    const mac_list = document.getElementById("mac_list");
    mac_string = mac_list.value;
    mac_string = mac_string.split(" ").join("");
    mac_string.split(",").forEach(mac => {
        console.log(mac);
        if (!isValidMACAddress(mac)) {
            valid = false;
        }
    });

    if (valid) {
        return true;
    } else {
        alert("Invalid MAC Address");
        return false;
    }
}