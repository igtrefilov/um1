function sendOptions() {
	const data = {
		option1: document.getElementById('option1').checked,
		option2: document.getElementById('option2').checked,
		option3: document.getElementById('option3').checked,
		option4: document.getElementById('option4').checked
	};

	fetch('/checkbox', {
	method: 'POST',
	headers: {
		'Content-Type': 'application/json'
	},
	body: JSON.stringify(data)
	})
	.then(response => {
		if (!response.ok) throw new Error("Ошибка при отправке");
		return response.text();
	})
	.then(result => {
		console.log("Ответ от сервера:", result);
	})
	.catch(error => {
		console.error("Ошибка:", error);
	});
  }
