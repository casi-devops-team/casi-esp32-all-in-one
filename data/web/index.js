const factoryResetButton = document.getElementById('factoryResetButton');
const resetConfirmationContainer = document.getElementById('reset-confirmation-container');
const confirmConfirmation = document.getElementById('reset-confirmation');
const confirmNo = document.getElementById('confirmNo');
const alertSuccess = document.getElementsByClassName('alert-success')[0];
const alertCloseButtons = document.getElementsByClassName('btn-close');

factoryResetButton.addEventListener('click', function () {
    resetConfirmationContainer.style.display = 'block';
});

confirmConfirmation.addEventListener('click', function () {
    // User clicked "Yes," do something here
    fetch("/factory-reset").then(x => {
        x.text().then((response) => {
            if (x.status == 200) {
                alertMessage = document.getElementById("alert-message");
                alertMessage.innerHTML = response;
                alertSuccess.classList.remove('alert-danger');
                alertSuccess.classList.add('alert-success');
                alertSuccess.style.display = 'flex';
            } else {
                alertMessage = document.getElementById("alert-message");
                alertMessage.innerHTML = response;
                alertSuccess.classList.remove('alert-success');
                alertSuccess.classList.add('alert-danger');
                alertSuccess.style.display = 'flex';
            }

        })

    }).catch((reason) => {
        console.log(reason);
        alertSuccess.classList.remove('alert-success');
        alertSuccess.classList.add('alert-danger');
        alertSuccess.style.display = 'flex';
    });
    resetConfirmationContainer.style.display = 'none';
});

[...alertCloseButtons].forEach((closeButton, index) => {
    closeButton.addEventListener('click', function () {
        alertSuccess.style.display = 'none';
    })
});

confirmNo.addEventListener('click', function () {
    // User clicked "No," do something here
    //console.log("You clicked 'No'!");
    resetConfirmationContainer.style.display = 'none';
});

window.addEventListener('click', function (event) {
    if (event.target.id === "reset-confirmation-model") {
        resetConfirmationContainer.style.display = 'none';
    }
});

