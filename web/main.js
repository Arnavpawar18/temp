const form = document.getElementById('plateForm');
const imageInput = document.getElementById('imageFile');
const imagePreview = document.getElementById('imagePreview');
const result = document.getElementById('result');

// Preview uploaded image
imageInput.addEventListener('change', function(e) {
  const file = e.target.files[0];
  if (file) {
    const reader = new FileReader();
    reader.onload = function(e) {
      imagePreview.src = e.target.result;
      imagePreview.style.display = 'block';
    };
    reader.readAsDataURL(file);
  }
});

// Handle form submission
form.addEventListener('submit', async function(e) {
  e.preventDefault();
  
  const formData = new FormData();
  const imageFile = imageInput.files[0];
  
  if (!imageFile) {
    result.textContent = 'Please select an image first';
    return;
  }
  
  formData.append('image', imageFile);
  result.textContent = 'Analyzing number plate...';
  
  try {
    const response = await fetch('/api/analyze-plate', {
      method: 'POST',
      body: formData
    });
    
    const data = await response.json();
    
    if (data.error) {
      result.textContent = `Error: ${data.error}`;
    } else {
      result.innerHTML = `<strong>Detected Plate:</strong> ${data.plate_text}`;
    }
  } catch (error) {
    result.textContent = `Error: ${error.message}`;
  }
});
